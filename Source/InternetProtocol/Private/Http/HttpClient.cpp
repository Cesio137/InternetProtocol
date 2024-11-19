// Fill out your copyright notice in the Description page of Project Settings.

#include "Http/HttpClient.h"

void UHttpClient::PreparePayload()
{
	Payload.Empty();

	Payload = Verb[Request.verb] + " " + Request.path;

	if (Request.params.Num() > 0)
	{
		Payload += "?";
		bool first = true;
		for (const TPair<FString, FString>& param : Request.params)
		{
			if (!first)
			{
				Payload += "&";
			}
			Payload += param.Key + "=" + param.Value;
			first = false;
		}
	}
	Payload += " HTTP/" + Request.version + "\r\n";

	Payload += "Host: " + Host;
	if (!Service.IsEmpty())
	{
		Payload += ":" + Service;
	}
	Payload += "\r\n";

	if (Request.headers.Num() > 0)
	{
		for (const TPair<FString, FString>& header : Request.headers)
		{
			Payload += header.Key + ": " + header.Value + "\r\n";
		}
		Payload += "Content-Length: " + FString::FromInt(Request.body.Len()) + "\r\n";
	}
	Payload += "\r\n";

	if (!Request.body.IsEmpty())
	{
		Payload += "\r\n" + Request.body;
	}
}

bool UHttpClient::AsyncPreparePayload()
{
	if (!ThreadPool.IsValid()) return false;
	asio::post(*ThreadPool, [=]()
	{
		MutexPayload.Lock();
		PreparePayload();
		OnAsyncPayloadFinished.Broadcast();
		MutexPayload.Unlock();
	});
	return true;
}

bool UHttpClient::ProcessRequest()
{
	if (!ThreadPool.IsValid() || !Payload.IsEmpty())
	{
		return false;
	}
	
	asio::post(*ThreadPool, std::bind(&UHttpClient::run_context_thread, this));
	return true;
}

void UHttpClient::CancelRequest()
{
	asio::error_code ec_shutdown;
	asio::error_code ec_close;
	TCP.context.stop();
	TCP.socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec_shutdown);
	TCP.socket.close(ec_close);
	if (ShouldStopContext) return;
	if (ec_shutdown)
	{
		ensureMsgf(!ec_shutdown, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ec_shutdown.value(),
				   ec_shutdown.message().c_str());
		OnError.Broadcast(ec_shutdown.value(), ec_shutdown.message().c_str());
	}
	if (ec_close)
	{
		ensureMsgf(!ec_close, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ec_close.value(),
				   ec_close.message().c_str());
		OnError.Broadcast(ec_close.value(), ec_close.message().c_str());
	}
	OnRequestCanceled.Broadcast();
}

void UHttpClient::run_context_thread()
{
	MutexIO.Lock();
	while (TCP.attemps_fail <= MaxAttemp)
	{
		if (ShouldStopContext) return;
		if (TCP.attemps_fail > 0)
		{
			OnRequestWillRetry.Broadcast(TCP.attemps_fail);
		}
		TCP.error_code.clear();
		TCP.context.restart();
		TCP.resolver.async_resolve(TCHAR_TO_UTF8(*GetHost()), TCHAR_TO_UTF8(*GetPort()),
		                           std::bind(&UHttpClient::resolve, this, asio::placeholders::error,
		                                     asio::placeholders::results)
		);
		TCP.context.run();
		if (!TCP.error_code)
		{
			break;
		}
		TCP.attemps_fail++;
		std::this_thread::sleep_for(std::chrono::seconds(Timeout));
	}
	consume_stream_buffers();
	TCP.attemps_fail = 0;
	MutexIO.Unlock();
}

void UHttpClient::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints) noexcept
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.

	asio::async_connect(TCP.socket, endpoints,
	                    std::bind(&UHttpClient::connect, this, asio::placeholders::error)
	);
}

void UHttpClient::connect(const std::error_code& error) noexcept
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// The connection was successful. Send the request.
	std::ostream request_stream(&RequestBuffer);
	request_stream << TCHAR_TO_UTF8(*Payload);
	asio::async_write(TCP.socket, RequestBuffer,
	                  std::bind(&UHttpClient::write_request, this, asio::placeholders::error,
	                            asio::placeholders::bytes_transferred)
	);
}

void UHttpClient::write_request(const std::error_code& error, const size_t bytes_sent) noexcept
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// Read the response status line. The response_ streambuf will
	// automatically grow to accommodate the entire line. The growth may be
	// limited by passing a maximum size to the streambuf constructor.
	OnRequestProgress.Broadcast(bytes_sent, bytes_sent);
	asio::async_read_until(TCP.socket, ResponseBuffer, "\r\n",
	                       std::bind(&UHttpClient::read_status_line, this, asio::placeholders::error, bytes_sent,
	                                 asio::placeholders::bytes_transferred)
	);
}

void UHttpClient::read_status_line(const std::error_code& error, const size_t bytes_sent, const size_t bytes_recvd) noexcept
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// Check that response is OK.
	OnRequestProgress.Broadcast(bytes_sent, bytes_recvd);
	std::istream response_stream(&ResponseBuffer);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		OnResponseError.Broadcast(1);
		return;
	}
	if (status_code != 200)
	{
		OnResponseError.Broadcast(status_code);
		return;
	}

	// Read the response headers, which are terminated by a blank line.
	asio::async_read_until(TCP.socket, ResponseBuffer, "\r\n\r\n",
	                       std::bind(&UHttpClient::read_headers, this, asio::placeholders::error)
	);
}

void UHttpClient::read_headers(const std::error_code& error) noexcept
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// Process the response headers.
	Response.clear();
	std::istream response_stream(&ResponseBuffer);
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
	{
		Response.appendHeader(header.data());
	}

	// Write whatever content we already have to output.
	std::ostringstream content_buffer;
	if (ResponseBuffer.size() > 0)
	{
		content_buffer << &ResponseBuffer;
		Response.setContent(content_buffer.str().data());
	}

	// Start reading remaining data until EOF.
	asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UHttpClient::read_content, this, asio::placeholders::error)
	);
}

void UHttpClient::read_content(const std::error_code& error) noexcept
{
	if (error)
	{
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestCompleted.Broadcast(Response);
		return;
	}
	// Write all of the data that has been read so far.
	std::ostringstream stream_buffer;
	stream_buffer << &ResponseBuffer;
	if (!stream_buffer.str().empty())
	{
		Response.appendContent(stream_buffer.str().data());
	}

	// Continue reading remaining data until EOF.
	asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UHttpClient::read_content, this, asio::placeholders::error)
	);
}

void UHttpClientSsl::PreparePayload()
{
	Payload.Empty();

	Payload = Verb[Request.verb] + " " + Request.path;

	if (Request.params.Num() > 0)
	{
		Payload += "?";
		bool first = true;
		for (const TPair<FString, FString>& param : Request.params)
		{
			if (!first)
			{
				Payload += "&";
			}
			Payload += param.Key + "=" + param.Value;
			first = false;
		}
	}
	Payload += " HTTP/" + Request.version + "\r\n";

	Payload += "Host: " + Host;
	if (!Service.IsEmpty())
	{
		Payload += ":" + Service;
	}
	Payload += "\r\n";

	if (Request.headers.Num() > 0)
	{
		for (const TPair<FString, FString>& header : Request.headers)
		{
			Payload += header.Key + ": " + header.Value + "\r\n";
		}
		Payload += "Content-Length: " + FString::FromInt(Request.body.Len()) + "\r\n";
	}
	Payload += "\r\n";

	if (!Request.body.IsEmpty())
	{
		Payload += "\r\n" + Request.body;
	}
}

bool UHttpClientSsl::AsyncPreparePayload()
{
	if (!ThreadPool.IsValid()) return false;
	asio::post(*ThreadPool, [=]()
	{
		MutexPayload.Lock();
		PreparePayload();
		OnAsyncPayloadFinished.Broadcast();
		MutexPayload.Unlock();
	});
	return true;
}

bool UHttpClientSsl::ProcessRequest()
{
	if (!ThreadPool.IsValid() || !Payload.IsEmpty())
	{
		return false;
	}

	asio::post(*ThreadPool, std::bind(&UHttpClientSsl::run_context_thread, this));
	return true;
}

void UHttpClientSsl::CancelRequest()
{
	TCP.context.stop();
	asio::error_code ec_shutdown;
	asio::error_code ec_close;
	TCP.ssl_socket.shutdown(ec_shutdown);
	TCP.ssl_socket.lowest_layer().close(ec_close);
	if (ShouldStopContext) return;
	if (ec_shutdown)
	{
		ensureMsgf(!ec_shutdown, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ec_shutdown.value(),
				   ec_shutdown.message().c_str());
		OnError.Broadcast(ec_shutdown.value(), ec_shutdown.message().c_str());
	}
	if (ec_close)
	{
		ensureMsgf(!ec_close, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ec_close.value(),
				   ec_close.message().c_str());
		OnError.Broadcast(ec_close.value(), ec_close.message().c_str());
	}
	OnRequestCanceled.Broadcast();
}

void UHttpClientSsl::run_context_thread()
{
	MutexIO.Lock();
	while (TCP.attemps_fail <= MaxAttemp)
	{
		if (ShouldStopContext) return;
		if (TCP.attemps_fail > 0)
		{
			OnRequestWillRetry.Broadcast(TCP.attemps_fail);
		}
		TCP.error_code.clear();
		TCP.context.restart();
		TCP.resolver.async_resolve(TCHAR_TO_UTF8(*GetHost()), TCHAR_TO_UTF8(*GetPort()),
								   std::bind(&UHttpClientSsl::resolve, this, asio::placeholders::error,
											 asio::placeholders::results)
		);
		TCP.context.run();
		if (!TCP.error_code)
		{
			break;
		}
		TCP.attemps_fail++;
		std::this_thread::sleep_for(std::chrono::seconds(Timeout));
	}
	consume_stream_buffers();
	TCP.attemps_fail = 0;
	MutexIO.Unlock();
}

void UHttpClientSsl::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.
	asio::async_connect(TCP.ssl_socket.lowest_layer(), endpoints,
						std::bind(&UHttpClientSsl::connect, this, asio::placeholders::error)
	);
}

void UHttpClientSsl::connect(const std::error_code& error)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// The connection was successful.
	TCP.ssl_socket.async_handshake(asio::ssl::stream_base::client,
										   std::bind(&UHttpClientSsl::ssl_handshake,
													 this, asio::placeholders::error));
}

void UHttpClientSsl::ssl_handshake(const std::error_code& error)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// The connection was successful. Send the request.
	std::ostream request_stream(&RequestBuffer);
	request_stream << TCHAR_TO_UTF8(*Payload);
	asio::async_write(TCP.ssl_socket, RequestBuffer,
					  std::bind(&UHttpClientSsl::write_request, this, asio::placeholders::error,
								asio::placeholders::bytes_transferred)
	);
}

void UHttpClientSsl::write_request(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// Read the response status line. The response_ streambuf will
	// automatically grow to accommodate the entire line. The growth may be
	// limited by passing a maximum size to the streambuf constructor.
	OnRequestProgress.Broadcast(bytes_sent, bytes_sent);
	asio::async_read_until(TCP.ssl_socket, ResponseBuffer, "\r\n",
						   std::bind(&UHttpClientSsl::read_status_line, this, asio::placeholders::error, bytes_sent,
									 asio::placeholders::bytes_transferred)
	);
}

void UHttpClientSsl::read_status_line(const std::error_code& error, const size_t bytes_sent, const size_t bytes_recvd)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// Check that response is OK.
	OnRequestProgress.Broadcast(bytes_sent, bytes_recvd);
	std::istream response_stream(&ResponseBuffer);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		OnResponseError.Broadcast(1);
		return;
	}
	if (status_code != 200)
	{
		OnResponseError.Broadcast(status_code);
		return;
	}

	// Read the response headers, which are terminated by a blank line.
	asio::async_read_until(TCP.ssl_socket, ResponseBuffer, "\r\n\r\n",
						   std::bind(&UHttpClientSsl::read_headers, this, asio::placeholders::error)
	);
}

void UHttpClientSsl::read_headers(const std::error_code& error)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// Process the response headers.
	Response.clear();
	std::istream response_stream(&ResponseBuffer);
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
	{
		Response.appendHeader(header.data());
	}

	// Write whatever content we already have to output.
	std::ostringstream content_buffer;
	if (ResponseBuffer.size() > 0)
	{
		content_buffer << &ResponseBuffer;
		Response.setContent(content_buffer.str().data());
	}

	// Start reading remaining data until EOF.
	asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
					 std::bind(&UHttpClientSsl::read_content, this, asio::placeholders::error)
	);
}

void UHttpClientSsl::read_content(const std::error_code& error)
{
	if (error)
	{
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnRequestCompleted.Broadcast(Response);
		return;
	}
	// Write all of the data that has been read so far.
	std::ostringstream stream_buffer;
	stream_buffer << &ResponseBuffer;
	if (!stream_buffer.str().empty())
	{
		Response.appendContent(stream_buffer.str().data());
	}

	// Continue reading remaining data until EOF.
	asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
					 std::bind(&UHttpClientSsl::read_content, this, asio::placeholders::error)
	);
}
