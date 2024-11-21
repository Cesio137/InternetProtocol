/*
 * Copyright (c) 2023-2024 Nathan Miguel
 *
 * InternetProtocol is free library: you can redistribute it and/or modify it under the terms
 * of the GNU Affero General Public License as published by the Free Software Foundation,
 * version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
*/

#include "Http/HttpClient.h"

#include "ChartCreation.h"
#include "InGamePerformanceTracker.h"

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
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnAsyncPayloadFinished.Broadcast();
		});
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
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				OnRequestWillRetry.Broadcast(TCP.attemps_fail);
			});
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
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	asio::async_connect(TCP.socket, endpoints,
	                    std::bind(&UHttpClient::connect, this, asio::placeholders::error)
	);
}

void UHttpClient::connect(const std::error_code& error) noexcept
{
	if (error)
	{
		TCP.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
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
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnRequestProgress.Broadcast(bytes_sent, bytes_sent);
	});
	asio::async_read_until(TCP.socket, ResponseBuffer, "\r\n",
	                       std::bind(&UHttpClient::read_status_line, this, asio::placeholders::error, bytes_sent,
	                                 asio::placeholders::bytes_transferred)
	);
}

void UHttpClient::read_status_line(const std::error_code& error, const size_t bytes_sent, const size_t bytes_recvd) noexcept
{
	if (error)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnRequestProgress.Broadcast(bytes_sent, bytes_recvd);
	});
	std::istream response_stream(&ResponseBuffer);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnResponseError.Broadcast(1);
		});
		return;
	}
	if (status_code != 200)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnResponseError.Broadcast(status_code);
		});
		return;
	}
	asio::async_read_until(TCP.socket, ResponseBuffer, "\r\n\r\n",
	                       std::bind(&UHttpClient::read_headers, this, asio::placeholders::error)
	);
}

void UHttpClient::read_headers(const std::error_code& error) noexcept
{
	if (error)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	Response.clear();
	std::istream response_stream(&ResponseBuffer);
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
	{
		Response.appendHeader(header.data());
	}
	std::ostringstream content_buffer;
	if (ResponseBuffer.size() > 0)
	{
		content_buffer << &ResponseBuffer;
		Response.setContent(content_buffer.str().data());
	}
	asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UHttpClient::read_content, this, asio::placeholders::error)
	);
}

void UHttpClient::read_content(const std::error_code& error) noexcept
{
	if (error)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestCompleted.Broadcast(Response);
		});
		return;
	}
	std::ostringstream stream_buffer;
	stream_buffer << &ResponseBuffer;
	if (!stream_buffer.str().empty())
	{
		Response.appendContent(stream_buffer.str().data());
	}
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
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnAsyncPayloadFinished.Broadcast();
		});
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
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				OnRequestWillRetry.Broadcast(TCP.attemps_fail);
			});
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
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	asio::async_connect(TCP.ssl_socket.lowest_layer(), endpoints,
						std::bind(&UHttpClientSsl::connect, this, asio::placeholders::error)
	);
}

void UHttpClientSsl::connect(const std::error_code& error)
{
	if (error)
	{
		TCP.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	TCP.ssl_socket.async_handshake(asio::ssl::stream_base::client,
										   std::bind(&UHttpClientSsl::ssl_handshake,
													 this, asio::placeholders::error));
}

void UHttpClientSsl::ssl_handshake(const std::error_code& error)
{
	if (error)
	{
		TCP.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
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
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnRequestProgress.Broadcast(bytes_sent, bytes_sent);
	});
	asio::async_read_until(TCP.ssl_socket, ResponseBuffer, "\r\n",
						   std::bind(&UHttpClientSsl::read_status_line, this, asio::placeholders::error, bytes_sent,
									 asio::placeholders::bytes_transferred)
	);
}

void UHttpClientSsl::read_status_line(const std::error_code& error, const size_t bytes_sent, const size_t bytes_recvd)
{
	if (error)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
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
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnResponseError.Broadcast(1);
		});
		return;
	}
	if (status_code != 200)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnResponseError.Broadcast(status_code);
		});
		return;
	}
	asio::async_read_until(TCP.ssl_socket, ResponseBuffer, "\r\n\r\n",
						   std::bind(&UHttpClientSsl::read_headers, this, asio::placeholders::error)
	);
}

void UHttpClientSsl::read_headers(const std::error_code& error)
{
	if (error)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	Response.clear();
	std::istream response_stream(&ResponseBuffer);
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
	{
		Response.appendHeader(header.data());
	}
	std::ostringstream content_buffer;
	if (ResponseBuffer.size() > 0)
	{
		content_buffer << &ResponseBuffer;
		Response.setContent(content_buffer.str().data());
	}
	asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
					 std::bind(&UHttpClientSsl::read_content, this, asio::placeholders::error)
	);
}

void UHttpClientSsl::read_content(const std::error_code& error)
{
	if (error)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestCompleted.Broadcast(Response);
		});
		return;
	}
	std::ostringstream stream_buffer;
	stream_buffer << &ResponseBuffer;
	if (!stream_buffer.str().empty())
	{
		Response.appendContent(stream_buffer.str().data());
	}
	asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
					 std::bind(&UHttpClientSsl::read_content, this, asio::placeholders::error)
	);
}
