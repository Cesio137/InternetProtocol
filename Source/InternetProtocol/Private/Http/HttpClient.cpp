/*
 * Copyright (c) 2023-2025 Nathan Miguel
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

void UHttpClient::PreparePayload()
{
	Payload.Empty();

	Payload = RequestMethod[Request.Method] + " " + Request.Path;

	if (Request.Params.Num() > 0)
	{
		Payload += "?";
		bool first = true;
		for (const TPair<FString, FString>& param : Request.Params)
		{
			if (!first)
			{
				Payload += "&";
			}
			Payload += param.Key + "=" + param.Value;
			first = false;
		}
	}
	Payload += " HTTP/" + Request.Version + "\r\n";

	Payload += "Host: " + Host;
	if (!Service.IsEmpty())
	{
		Payload += ":" + Service;
	}
	Payload += "\r\n";

	if (Request.Headers.Num() > 0)
	{
		for (const TPair<FString, FString>& header : Request.Headers)
		{
			Payload += header.Key + ": " + header.Value + "\r\n";
		}
		Payload += "Content-Length: " + FString::FromInt(Request.Body.Len()) + "\r\n";
	}
	Payload += "\r\n";

	if (!Request.Body.IsEmpty())
	{
		Payload += "\r\n" + Request.Body;
	}
}

bool UHttpClient::AsyncPreparePayload()
{
	if (!ThreadPool.IsValid()) return false;
	asio::post(GetThreadPool(), [&]()
	{
		FScopeLock Guard(&MutexPayload);
		MutexPayload.Lock();
		PreparePayload();
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnAsyncPayloadFinished.Broadcast();
		});
	});
	return true;
}

bool UHttpClient::ProcessRequest()
{
	if (Payload.IsEmpty())
	{
		return false;
	}
	
	asio::post(GetThreadPool(), std::bind(&UHttpClient::run_context_thread, this));
	return true;
}

void UHttpClient::Close()
{
	IsClosing = true;
	TCP.context.stop();
	if (TCP.socket.is_open()) {
		TCP.socket.shutdown(asio::ip::udp::socket::shutdown_both, ErrorCode);
		if (ErrorCode) {
			FScopeLock Guard(&MutexError);
			OnError.Broadcast(ErrorCode);
		}

		TCP.socket.close(ErrorCode);
		if (ErrorCode) {
			FScopeLock Guard(&MutexError);
			OnError.Broadcast(ErrorCode);
		}
	}
	TCP.context.restart();
	OnClose.Broadcast();
	IsClosing  = false;
}

void UHttpClient::consume_stream_buffers()
{
	const size_t req_size = RequestBuffer.size();
	if (req_size > 0)
		RequestBuffer.consume(req_size);
	const size_t res_size = RequestBuffer.size();
	if (res_size > 0)
		RequestBuffer.consume(res_size);
}

void UHttpClient::run_context_thread()
{
	if (TCP.socket.is_open()) {
		std::ostream request_stream(&RequestBuffer);
		request_stream << TCHAR_TO_UTF8(*Payload);
		asio::async_write(TCP.socket, RequestBuffer,
					  std::bind(&UHttpClient::write_request, this, asio::placeholders::error,
								asio::placeholders::bytes_transferred, false));
		return;
	}
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	TCP.resolver.async_resolve(TCHAR_TO_UTF8(*Host), TCHAR_TO_UTF8(*Service),
								   std::bind(&UHttpClient::resolve, this, asio::placeholders::error,
											 asio::placeholders::results));
	TCP.context.run();
	if (TCP.socket.is_open() || !IsClosing)
		Close();
	else if (TCP.context.stopped() && !IsClosing)
		TCP.context.restart();
}

void UHttpClient::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error) return;
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
		});
		return;
	}
	asio::async_connect(TCP.socket, endpoints,
	                    std::bind(&UHttpClient::connect, this, asio::placeholders::error)
	);
}

void UHttpClient::connect(const std::error_code& error)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error) return;
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
		});
		return;
	}
	std::ostream request_stream(&RequestBuffer);
	request_stream << TCHAR_TO_UTF8(*Payload);
	asio::async_write(TCP.socket, RequestBuffer,
	                  std::bind(&UHttpClient::write_request, this, asio::placeholders::error,
	                            asio::placeholders::bytes_transferred, true));
}

void UHttpClient::write_request(const std::error_code& error, const size_t bytes_sent, const bool trigger_read_until)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error) return;
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent]()
	{
		OnRequestProgress.Broadcast(bytes_sent, 0);
	});
	if (trigger_read_until)
		asio::async_read_until(TCP.socket, ResponseBuffer, "\r\n",
		                       std::bind(&UHttpClient::read_status_line, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
}

void UHttpClient::read_status_line(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error) return;
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd]()
	{
		OnRequestProgress.Broadcast(0, bytes_recvd);
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
		FScopeLock Guard(&MutexError);
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnResponseError.Broadcast(505, ResponseStatusCode[505]);
		});
		return;
	}
	if (status_code != 200)
	{
		FScopeLock Guard(&MutexError);
		AsyncTask(ENamedThreads::GameThread, [&, status_code]()
		{
			OnResponseError.Broadcast(status_code, ResponseStatusCode.Contains(status_code) ? ResponseStatusCode[status_code] : "");
		});
		return;
	}
	asio::async_read_until(TCP.socket, ResponseBuffer, "\r\n\r\n",
	                       std::bind(&UHttpClient::read_headers, this, asio::placeholders::error)
	);
}

void UHttpClient::read_headers(const std::error_code& error)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error) return;
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
		});
		return;
	}
	UHttpFunctionLibrary::ClearResponse(Response);
	std::istream response_stream(&ResponseBuffer);
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
	{
		UHttpFunctionLibrary::AppendHeader(Response, header.data());
	}
	std::ostringstream body_buffer;
	
	if (ResponseBuffer.size() > 0)
	{
		body_buffer << &ResponseBuffer;
		UHttpFunctionLibrary::SetBody(Response, body_buffer.str().data());
	}

	std::ostringstream stream_buffer;
	stream_buffer << &ResponseBuffer;
	if (!stream_buffer.str().empty()) UHttpFunctionLibrary::AppendBody(Response, stream_buffer.str().c_str());
	if (ResponseBuffer.size() > 0) {
		asio::async_read(
			TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
			std::bind(&UHttpClient::read_body, this, asio::placeholders::error));
		return;
	}
	const FClientResponse res = Response;
	AsyncTask(ENamedThreads::GameThread, [&, res]()
	{
		OnRequestCompleted.Broadcast(res);
	});
	consume_stream_buffers();
	if (Response.Headers["Connection"] == "close") {
		if (!IsClosing) Close();
	} else {
		asio::async_read_until(TCP.socket, ResponseBuffer, "\r\n",
							   std::bind(&UHttpClient::read_status_line, this,
										 asio::placeholders::error, asio::placeholders::bytes_transferred));
	}
}

void UHttpClient::read_body(const std::error_code& error)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
		});
		return;
	}
	std::ostringstream body_buffer;
	body_buffer << &ResponseBuffer;
	if (!body_buffer.str().empty()) UHttpFunctionLibrary::AppendBody(Response, body_buffer.str().c_str());
	if (ResponseBuffer.size() > 0) {
		asio::async_read(
			TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
			std::bind(&UHttpClient::read_body, this, asio::placeholders::error));
		return;
	}
	const FClientResponse res = Response;
	AsyncTask(ENamedThreads::GameThread, [&, res]()
	{
		OnRequestCompleted.Broadcast(res);
	});
	consume_stream_buffers();
	if (Response.Headers["Connection"] == "close") {
		if (!IsClosing) Close();
	} else {
		asio::async_read_until(TCP.socket, ResponseBuffer, "\r\n",
							   std::bind(&UHttpClient::read_status_line, this,
										 asio::placeholders::error, asio::placeholders::bytes_transferred));
	}
}

void UHttpClientSsl::PreparePayload()
{
	Payload.Empty();

	Payload = RequestMethod[Request.Method] + " " + Request.Path;

	if (Request.Params.Num() > 0)
	{
		Payload += "?";
		bool first = true;
		for (const TPair<FString, FString>& param : Request.Params)
		{
			if (!first)
			{
				Payload += "&";
			}
			Payload += param.Key + "=" + param.Value;
			first = false;
		}
	}
	Payload += " HTTP/" + Request.Version + "\r\n";

	Payload += "Host: " + Host;
	if (!Service.IsEmpty())
	{
		Payload += ":" + Service;
	}
	Payload += "\r\n";

	if (Request.Headers.Num() > 0)
	{
		for (const TPair<FString, FString>& header : Request.Headers)
		{
			Payload += header.Key + ": " + header.Value + "\r\n";
		}
		Payload += "Content-Length: " + FString::FromInt(Request.Body.Len()) + "\r\n";
	}
	Payload += "\r\n";

	if (!Request.Body.IsEmpty())
	{
		Payload += "\r\n" + Request.Body;
	}
}

bool UHttpClientSsl::AsyncPreparePayload()
{
	asio::post(GetThreadPool(), [&]()
	{
		FScopeLock Guard(&MutexPayload);
		PreparePayload();
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnAsyncPayloadFinished.Broadcast();
		});
	});
	return true;
}

bool UHttpClientSsl::ProcessRequest()
{
	if (Payload.IsEmpty())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UHttpClientSsl::run_context_thread, this));
	return true;
}

void UHttpClientSsl::Close()
{
	IsClosing = true;
	if (TCP.ssl_socket.next_layer().is_open()) {
		TCP.ssl_socket.shutdown(ErrorCode);
		if (ErrorCode) {
			FScopeLock Guard(&MutexError);
			OnError.Broadcast(ErrorCode);
		}
		TCP.ssl_socket.next_layer().close(ErrorCode);
		if (ErrorCode) {
			FScopeLock Guard(&MutexError);
			OnError.Broadcast(ErrorCode);
		}
	}
	TCP.context.stop();
	TCP.context.restart();
	TCP.ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(TCP.context, TCP.ssl_context);
	OnError.Broadcast(ErrorCode);
	IsClosing = false;
}

void UHttpClientSsl::run_context_thread()
{
	if (TCP.ssl_socket.next_layer().is_open()) {
		std::ostream request_stream(&RequestBuffer);
		request_stream << TCHAR_TO_UTF8(*Payload);
		asio::async_write(TCP.ssl_socket, RequestBuffer,
						  std::bind(&UHttpClientSsl::write_request, this,
									asio::placeholders::error,
									asio::placeholders::bytes_transferred, false));
		return;
	}
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	TCP.resolver.async_resolve(TCHAR_TO_UTF8(*Host), TCHAR_TO_UTF8(*Service),
								   std::bind(&UHttpClientSsl::resolve, this, asio::placeholders::error,
											 asio::placeholders::results));
	TCP.context.run();
	TCP.context.restart();
	if (TCP.ssl_socket.next_layer().is_open() && !IsClosing) {
		Close();
	} else if (TCP.context.stopped() && !IsClosing) {
		TCP.context.restart();
		TCP.ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(TCP.context, TCP.ssl_context);
	}
}

void UHttpClientSsl::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error) return;
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
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
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error) return;
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
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
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error) return;
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
		});
		return;
	}
	std::ostream request_stream(&RequestBuffer);
	request_stream << TCHAR_TO_UTF8(*Payload);
	asio::async_write(TCP.ssl_socket, RequestBuffer,
					  std::bind(&UHttpClientSsl::write_request, this, asio::placeholders::error,
								asio::placeholders::bytes_transferred, true));
}

void UHttpClientSsl::write_request(const std::error_code& error, const size_t bytes_sent, const bool trigger_read_until)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error) return;
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent]()
	{
		OnRequestProgress.Broadcast(bytes_sent, 0);
	});
	if (trigger_read_until)
		asio::async_read_until(TCP.ssl_socket, ResponseBuffer, "\r\n",
							   std::bind(&UHttpClientSsl::read_status_line, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
}

void UHttpClientSsl::read_status_line(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error) return;
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd]()
	{
		OnRequestProgress.Broadcast(0, bytes_recvd);
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
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnResponseError.Broadcast(505, ResponseStatusCode[505]);
		});
		return;
	}
	if (status_code != 200)
	{
		AsyncTask(ENamedThreads::GameThread, [&, status_code]()
		{
			OnResponseError.Broadcast(status_code, ResponseStatusCode.Contains(status_code) ? ResponseStatusCode[status_code] : "");
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
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error) return;
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
		});
		return;
	}
	UHttpFunctionLibrary::ClearResponse(Response);
	std::istream response_stream(&ResponseBuffer);
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
	{
		UHttpFunctionLibrary::AppendHeader(Response, header.data());
	}
	std::ostringstream body_buffer;
	
	if (ResponseBuffer.size() > 0)
	{
		body_buffer << &ResponseBuffer;
		UHttpFunctionLibrary::SetBody(Response, body_buffer.str().data());
	}

	std::ostringstream stream_buffer;
	stream_buffer << &ResponseBuffer;
	if (!stream_buffer.str().empty()) UHttpFunctionLibrary::AppendBody(Response, stream_buffer.str().c_str());
	if (ResponseBuffer.size() > 0) {
		asio::async_read(
			TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
			std::bind(&UHttpClientSsl::read_body, this, asio::placeholders::error));
		return;
	}
	const FClientResponse res = Response;
	AsyncTask(ENamedThreads::GameThread, [&, res]()
	{
		OnRequestCompleted.Broadcast(res);
	});
	consume_stream_buffers();
	if (Response.Headers["Connection"] == "close") {
		if (!IsClosing) Close();
	} else {
		asio::async_read_until(TCP.ssl_socket, ResponseBuffer, "\r\n",
							   std::bind(&UHttpClientSsl::read_status_line, this,
										 asio::placeholders::error, asio::placeholders::bytes_transferred));
	}
}

void UHttpClientSsl::read_body(const std::error_code& error)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error) return;
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error);
		});
		return;
	}
	std::ostringstream body_buffer;
	body_buffer << &ResponseBuffer;
	if (!body_buffer.str().empty()) UHttpFunctionLibrary::AppendBody(Response, body_buffer.str().c_str());
	if (ResponseBuffer.size() > 0) {
		asio::async_read(
			TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
			std::bind(&UHttpClientSsl::read_body, this, asio::placeholders::error));
		return;
	}
	const FClientResponse res = Response;
	AsyncTask(ENamedThreads::GameThread, [&, res]()
	{
		OnRequestCompleted.Broadcast(res);
	});
	consume_stream_buffers();
	if (Response.Headers["Connection"] == "close") {
		if (!IsClosing) Close();
	} else {
		asio::async_read_until(TCP.ssl_socket, ResponseBuffer, "\r\n",
							   std::bind(&UHttpClientSsl::read_status_line, this,
										 asio::placeholders::error, asio::placeholders::bytes_transferred));
	}
}
