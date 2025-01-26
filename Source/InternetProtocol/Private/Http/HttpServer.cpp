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

#include "Http/HttpServer.h"
#include "Library/InternetProtocolFunctionLibrary.h"

bool UHttpServer::SendResponse(const FServerResponse& Response, const FTCPSocket& Socket)
{
	if (!Socket.RawPtr->is_open())
	{
		return false;
	}
	asio::post(GetThreadPool(), std::bind(&UHttpServer::process_response, this, Response, Socket.SmartPtr));
	return true;
}

bool UHttpServer::SendErrorResponse(const int StatusCode, const FTCPSocket& Socket)
{
	if (!Socket.RawPtr->is_open())
	{
		return false;
	}
	asio::post(GetThreadPool(), std::bind(&UHttpServer::process_error_response, this, StatusCode, Socket.SmartPtr));
	return true;
}

bool UHttpServer::Open()
{
	if (TCP.acceptor.is_open())
	{
		return false;
	}

	asio::ip::tcp::endpoint endpoint(TcpProtocol == EProtocolType::V4
		                                 ? asio::ip::tcp::v4()
		                                 : asio::ip::tcp::v6(), TcpPort);
	ErrorCode = asio::error_code();
	TCP.acceptor.open(TcpProtocol == EProtocolType::V4
		                  ? asio::ip::tcp::v4()
		                  : asio::ip::tcp::v6(), ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	TCP.acceptor.set_option(asio::socket_base::reuse_address(true), ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	TCP.acceptor.bind(endpoint, ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	TCP.acceptor.listen(Backlog, ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UHttpServer::run_context_thread, this));
	return true;
}

void UHttpServer::Close()
{
	IsClosing = true;
	if (TCP.sockets.Num() > 0)
	{
		for (const socket_ptr& socket : TCP.sockets)
		{
			if (socket->is_open())
			{
				socket->shutdown(asio::ip::tcp::socket::shutdown_both, ErrorCode);
				if (ErrorCode)
				{
					FScopeLock Guard(&MutexError);
					OnSocketError.Broadcast(ErrorCode, socket);
				}
				socket->close(ErrorCode);
				if (ErrorCode)
				{
					FScopeLock Guard(&MutexError);
					OnSocketError.Broadcast(ErrorCode, socket);
				}
			}
		}
	}
	TCP.context.stop();
	if (TCP.sockets.Num() > 0)
	{
		TCP.sockets.Empty();
	}
	if (RequestBuffers.Num() > 0)
	{
		RequestBuffers.Empty();
	}
	if (TCP.acceptor.is_open())
	{
		TCP.acceptor.close(ErrorCode);
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
	}
	TCP.context.restart();
	TCP.acceptor = asio::ip::tcp::acceptor(TCP.context);
	OnClose.Broadcast();
	IsClosing = false;
}

void UHttpServer::DisconnectSocket(const FTCPSocket& socket)
{
	if (socket.RawPtr->is_open())
	{
		socket.RawPtr->shutdown(asio::ip::tcp::socket::shutdown_both, ErrorCode);
		if (ErrorCode)
		{
			OnSocketError.Broadcast(ErrorCode, socket.RawPtr);
		}
		socket.RawPtr->close(ErrorCode);
		if (ErrorCode)
		{
			OnSocketError.Broadcast(ErrorCode, socket.RawPtr);
		}
	}
	if (RequestBuffers.Contains(socket.SmartPtr))
	{
		RequestBuffers.Remove(socket.SmartPtr);
	}
	if (TCP.sockets.Contains(socket.SmartPtr))
	{
		TCP.sockets.Remove(socket.SmartPtr);
	}
	OnSocketDisconnected.Broadcast(ErrorCode, socket.SmartPtr);
}

void UHttpServer::process_response(FServerResponse& response, const socket_ptr& socket)
{
	FString payload = "HTTP/" + response.Version + " 200 OK\r\n";
	if (response.Headers.Num() > 0)
	{
		for (const TPair<FString, FString> header : response.Headers)
		{
			payload += header.Key + ": " + header.Value + "\r\n";
		}
		payload += "Content-Length: " + FString::FromInt(response.Body.Len()) + "\r\n";
	}
	payload += "\r\n";
	if (!response.Body.IsEmpty())
	{
		payload += response.Body + "\r\n";
	}

	const bool should_close = response.Headers.Contains("Connection")
		                          ? (response.Headers["Connection"] == "close")
		                          : true;
	std::string utf8payload = TCHAR_TO_UTF8(*payload);
	asio::async_write(
		*socket, asio::buffer(utf8payload.data(), utf8payload.size()),
		std::bind(&UHttpServer::write_response, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred, socket, should_close));
}

void UHttpServer::process_error_response(const int status_code, const socket_ptr& socket)
{
	FString payload = "HTTP/1.1 " + FString::FromInt(status_code) + " " + ResponseStatusCode[status_code] +
		"\r\n";
	std::string utf8payload = TCHAR_TO_UTF8(*payload);
	asio::async_write(
		*socket, asio::buffer(utf8payload.data(), utf8payload.size()),
		std::bind(&UHttpServer::write_response, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred, socket, true));
}

void UHttpServer::consume_request_buffer(const socket_ptr& socket)
{
	if (!RequestBuffers.Contains(socket))
	{
		return;
	}
	asio::streambuf& streambuf = *RequestBuffers[socket];
	const size_t size = streambuf.size();
	if (size > 0)
	{
		streambuf.consume(size);
	}
}

void UHttpServer::run_context_thread()
{
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	TSharedPtr<asio::ip::tcp::socket> conn_socket = MakeShared<asio::ip::tcp::socket>(TCP.context);
	TCP.acceptor.async_accept(
		*conn_socket, std::bind(&UHttpServer::accept, this, asio::placeholders::error, conn_socket));
	TCP.context.run();
	if (TCP.acceptor.is_open() && !IsClosing)
	{
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			Close();
		});
	}
}

void UHttpServer::accept(const std::error_code& error, socket_ptr& socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnSocketError.Broadcast(error, socket);
		});
		TSharedPtr<asio::ip::tcp::socket> conn_socket = MakeShared<asio::ip::tcp::socket>(TCP.context);
		TCP.acceptor.async_accept(
			*conn_socket, std::bind(&UHttpServer::accept, this, asio::placeholders::error, conn_socket));
		return;
	}

	TCP.sockets.Add(socket);
	TSharedPtr<asio::streambuf> response_buffer = MakeShared<asio::streambuf>();
	RequestBuffers.Add(socket, response_buffer);
	asio::async_read(*socket, *RequestBuffers[socket], asio::transfer_at_least(1),
	                 std::bind(&UHttpServer::read_status_line, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred, socket));

	AsyncTask(ENamedThreads::GameThread, [&, socket]()
	{
		OnSocketAccepted.Broadcast(socket);
	});

	TSharedPtr<asio::ip::tcp::socket> conn_socket = MakeShared<asio::ip::tcp::socket>(TCP.context);
	TCP.acceptor.async_accept(
		*conn_socket, std::bind(&UHttpServer::accept, this, asio::placeholders::error, conn_socket));
}

void UHttpServer::write_response(const asio::error_code& error, const size_t bytes_sent, const socket_ptr& socket,
                                 const bool close_connection)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnResponseSent.Broadcast(error);
			OnSocketError.Broadcast(error, socket);
		});
		return;
	}

	if (close_connection)
	{
		AsyncTask(ENamedThreads::GameThread, [&, socket]()
		{
			DisconnectSocket(socket);
		});
	}
	else
	{
		asio::async_read_until(*socket, *RequestBuffers[socket], "\r\n",
		                       std::bind(&UHttpServer::read_status_line, this,
		                                 asio::placeholders::error, asio::placeholders::bytes_transferred,
		                                 socket));
	}

	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent, error]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
		OnResponseSent.Broadcast(error);
	});
}

void UHttpServer::read_status_line(const asio::error_code& error, const size_t bytes_recvd, const socket_ptr& socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnSocketError.Broadcast(error, socket);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
	});
	std::istream response_stream(&*RequestBuffers[socket]);
	std::string method;
	response_stream >> method;
	std::string path;
	response_stream >> path;
	std::string version;
	response_stream >> version;

	std::string error_payload;
	std::set<std::string> versions = {"HTTP/1.0", "HTTP/1.1", "HTTP/2.0"};
	if (!ServerRequestMethod.Contains(UTF8_TO_TCHAR(method.c_str())))
	{
		if (versions.find(version) == versions.end())
		{
			version = "HTTP/1.1";
		}
		error_payload = version + " 405 Method Not Allowed\r\n";
		if (Headers.Num() > 0)
		{
			for (const TPair<FString, FString> header : Headers)
			{
				std::string key, val;
				key = TCHAR_TO_UTF8(*header.Key); val = TCHAR_TO_UTF8(*header.Value);
				error_payload += key + ": " + val + "\r\n";
			}
		}
		error_payload += "Allow: DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT, TRACE\r\n";
		error_payload += "Content-Length: 0\r\n";

		asio::async_write(
			*socket, asio::buffer(error_payload.data(), error_payload.size()),
			std::bind(&UHttpServer::write_response, this, asio::placeholders::error,
			          asio::placeholders::bytes_transferred, socket, true));
		return;
	}
	if (versions.find(version) == versions.end())
	{
		error_payload = "HTTP/1.1 505 HTTP Version Not Supported\r\n";
		if (Headers.Num() > 0)
		{
			for (const TPair<FString, FString> header : Headers)
			{
				std::string key, val;
				key = TCHAR_TO_UTF8(*header.Key); val = TCHAR_TO_UTF8(*header.Value);
				error_payload += key + ": " + val + "\r\n";
			}
		}
		error_payload += "Content-Length: 0\r\n";

		asio::streambuf res_buffer;
		std::ostream request_stream(&res_buffer);
		request_stream << error_payload;
		asio::async_write(
			*socket, res_buffer,
			std::bind(&UHttpServer::write_response, this, asio::placeholders::error,
			          asio::placeholders::bytes_transferred, socket, true));
		return;
	}

	FServerRequest request;
	version.erase(0, 5);
	request.Version = UTF8_TO_TCHAR(version.c_str());
	request.Method = ServerRequestMethod[UTF8_TO_TCHAR(method.c_str())];
	request.Path = UTF8_TO_TCHAR(path.c_str());

	RequestBuffers[socket]->consume(2);
	asio::async_read_until(
		*socket, *RequestBuffers[socket], "\r\n\r\n",
		std::bind(&UHttpServer::read_headers, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred, request, socket));
}

void UHttpServer::read_headers(const asio::error_code& error, const size_t bytes_recvd, FServerRequest& request,
	const socket_ptr& socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error, socket);
		});
		return;
	}
	std::istream response_stream(RequestBuffers[socket].Get());
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
		UHttpFunctionLibrary::ServerAppendHeader(request, UTF8_TO_TCHAR(header.c_str()));

	read_body(request, socket);
}

void UHttpServer::read_body(FServerRequest& request, const socket_ptr& socket)
{
	if (RequestBuffers[socket]->size() == 0) {
		FServerResponse response;
		response.Version = request.Version;
		response.Headers = Headers;
		if (request.Headers.Contains("Connection"))
			response.Headers["Connection"] = request.Headers["Connection"];
		AsyncTask(ENamedThreads::GameThread, [&, request, response, socket]()
		{
			OnRequestReceived.Broadcast(request, response, socket);
		});
		consume_request_buffer(socket);
		return;
	}

	std::ostringstream body_buffer;
	body_buffer << RequestBuffers[socket].Get();
	UHttpFunctionLibrary::ServerSetBody(request, body_buffer.str().c_str());
	
	FServerResponse response;
	response.Version = request.Version;
	response.Headers = Headers;
	if (request.Headers.Contains("Connection"))
		response.Headers["Connection"] = request.Headers["Connection"];
	AsyncTask(ENamedThreads::GameThread, [&, request, response, socket]()
	{
		OnRequestReceived.Broadcast(request, response, socket);
	});
	consume_request_buffer(socket);
}

bool UHttpServerSsl::SendResponse(const FServerResponse& Response, const FTCPSslSocket& SslSocket)
{
	if (!SslSocket.RawPtr->lowest_layer().is_open())
	{
		return false;
	}
	asio::post(GetThreadPool(), std::bind(&UHttpServerSsl::process_response, this, Response, SslSocket.SmartPtr));
	return true;
}

bool UHttpServerSsl::SendErrorResponse(const int StatusCode, const FTCPSslSocket& SslSocket)
{
	if (!SslSocket.RawPtr->lowest_layer().is_open())
	{
		return false;
	}
	asio::post(GetThreadPool(), std::bind(&UHttpServerSsl::process_error_response, this, StatusCode, SslSocket.SmartPtr));
	return true;
}

bool UHttpServerSsl::Open()
{
	if (TCP.acceptor.is_open())
	{
		return false;
	}

	asio::ip::tcp::endpoint endpoint(TcpProtocol == EProtocolType::V4
										 ? asio::ip::tcp::v4()
										 : asio::ip::tcp::v6(), TcpPort);
	ErrorCode = asio::error_code();
	TCP.acceptor.open(TcpProtocol == EProtocolType::V4
						  ? asio::ip::tcp::v4()
						  : asio::ip::tcp::v6(), ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	TCP.acceptor.set_option(asio::socket_base::reuse_address(true), ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	TCP.acceptor.bind(endpoint, ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	TCP.acceptor.listen(Backlog, ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UHttpServerSsl::run_context_thread, this));
	return true;
}

void UHttpServerSsl::Close()
{
	IsClosing = true;
	if (TCP.ssl_sockets.Num() > 0)
		for (const ssl_socket_ptr &ssl_socket: TCP.ssl_sockets) {
			if (ssl_socket->next_layer().is_open()) {
				asio::error_code ec;
				ssl_socket->shutdown(ec);
				ssl_socket->next_layer().close(ec);
			}
		}
	TCP.context.stop();
	if (TCP.ssl_sockets.Num() > 0) TCP.ssl_sockets.Empty();
	if (RequestBuffers.Num() > 0) RequestBuffers.Empty();
	if (TCP.acceptor.is_open()) {
		TCP.acceptor.close(ErrorCode); {
			FScopeLock Guard(&MutexError);
			OnError.Broadcast(ErrorCode);
		}
	}
	TCP.context.restart();
	TCP.acceptor = asio::ip::tcp::acceptor(TCP.context);
	OnClose.Broadcast();
	IsClosing = false;
}

void UHttpServerSsl::DisconnectSocket(const FTCPSslSocket& SslSocket)
{
	if (SslSocket.RawPtr->next_layer().is_open()) {
		SslSocket.RawPtr->shutdown(ErrorCode);
		SslSocket.RawPtr->next_layer().close(ErrorCode);
	}
	if (RequestBuffers.Contains(SslSocket.SmartPtr))
		RequestBuffers.Remove(SslSocket.SmartPtr);
	if (TCP.ssl_sockets.Contains(SslSocket.SmartPtr))
		TCP.ssl_sockets.Remove(SslSocket.SmartPtr);
	OnSocketDisconnected.Broadcast(ErrorCode, SslSocket);
}

void UHttpServerSsl::process_response(FServerResponse& response, const ssl_socket_ptr& ssl_socket)
{
	FString payload = "HTTP/" + response.Version + " 200 OK\r\n";
	if (response.Headers.Num() > 0)
	{
		for (const TPair<FString, FString> header : response.Headers)
		{
			payload += header.Key + ": " + header.Value + "\r\n";
		}
		payload += "Content-Length: " + FString::FromInt(response.Body.Len()) + "\r\n";
	}
	payload += "\r\n";
	if (!response.Body.IsEmpty())
	{
		payload += response.Body + "\r\n";
	}

	const bool should_close = response.Headers.Contains("Connection")
								  ? (response.Headers["Connection"] == "close")
								  : true;
	std::string utf8payload = TCHAR_TO_UTF8(*payload);
	asio::async_write(
		*ssl_socket, asio::buffer(utf8payload.data(), utf8payload.size()),
		std::bind(&UHttpServerSsl::write_response, this, asio::placeholders::error,
				  asio::placeholders::bytes_transferred, ssl_socket, should_close));
}

void UHttpServerSsl::process_error_response(const int status_code, const ssl_socket_ptr& ssl_socket)
{
	FString payload = "HTTP/1.1 " + FString::FromInt(status_code) + " " + ResponseStatusCode[status_code] +
		"\r\n";
	std::string utf8payload = TCHAR_TO_UTF8(*payload);
	asio::async_write(
		*ssl_socket, asio::buffer(utf8payload.data(), utf8payload.size()),
		std::bind(&UHttpServerSsl::write_response, this, asio::placeholders::error,
				  asio::placeholders::bytes_transferred, ssl_socket, true));
}

void UHttpServerSsl::consume_request_buffer(const ssl_socket_ptr& ssl_socket)
{
	if (!RequestBuffers.Contains(ssl_socket))
	{
		return;
	}
	asio::streambuf& streambuf = *RequestBuffers[ssl_socket];
	const size_t size = streambuf.size();
	if (size > 0)
	{
		streambuf.consume(size);
	}
}

void UHttpServerSsl::run_context_thread()
{
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	ssl_socket_ptr ssl_conn_socket = MakeShared<asio::ssl::stream<asio::ip::tcp::socket>>(TCP.context, TCP.ssl_context);
	TCP.acceptor.async_accept(
		ssl_conn_socket->lowest_layer(), std::bind(&UHttpServerSsl::accept, this, asio::placeholders::error, ssl_conn_socket));
	TCP.context.run();
	if (TCP.acceptor.is_open() && !IsClosing)
	{
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			Close();
		});
	}
}

void UHttpServerSsl::accept(const std::error_code& error, ssl_socket_ptr& ssl_socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, ssl_socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error, ssl_socket);
		});
		ssl_socket_ptr ssl_conn_socket = MakeShared<asio::ssl::stream<asio::ip::tcp::socket>>(TCP.context, TCP.ssl_context);
		TCP.acceptor.async_accept(ssl_conn_socket->lowest_layer(),
								  std::bind(&UHttpServerSsl::accept, this, asio::placeholders::error,
											ssl_conn_socket));
		return;
	}

	ssl_socket->async_handshake(asio::ssl::stream_base::server,
										std::bind(&UHttpServerSsl::ssl_handshake, this,
												  asio::placeholders::error, ssl_socket));

	
	ssl_socket_ptr ssl_conn_socket = MakeShared<asio::ssl::stream<asio::ip::tcp::socket>>(TCP.context, TCP.ssl_context);
	TCP.acceptor.async_accept(ssl_conn_socket->lowest_layer(),
							  std::bind(&UHttpServerSsl::accept, this, asio::placeholders::error,
										ssl_conn_socket));
}

void UHttpServerSsl::ssl_handshake(const asio::error_code& error, ssl_socket_ptr& ssl_socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, ssl_socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error, ssl_socket);
		});
		return;
	}
	
	TCP.ssl_sockets.Add(ssl_socket);
	TSharedPtr<asio::streambuf> response_buffer = MakeShared<asio::streambuf>();
	RequestBuffers.Add(ssl_socket, response_buffer);
	asio::async_read(*ssl_socket, *RequestBuffers[ssl_socket], asio::transfer_at_least(1),
					 std::bind(&UHttpServerSsl::read_status_line, this, asio::placeholders::error,
							   asio::placeholders::bytes_transferred, ssl_socket));

	AsyncTask(ENamedThreads::GameThread, [&, ssl_socket]()
	{
		OnSocketAccepted.Broadcast(ssl_socket);
	});

}

void UHttpServerSsl::write_response(const asio::error_code& error, const size_t bytes_sent,
	const ssl_socket_ptr& ssl_socket, const bool close_connection)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, ssl_socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnResponseSent.Broadcast(error);
			OnSocketError.Broadcast(error, ssl_socket);
		});
		return;
	}

	if (close_connection)
	{
		AsyncTask(ENamedThreads::GameThread, [&, ssl_socket]()
		{
			DisconnectSocket(ssl_socket);
		});
	}
	else
	{
		asio::async_read_until(*ssl_socket, *RequestBuffers[ssl_socket], "\r\n",
							   std::bind(&UHttpServerSsl::read_status_line, this,
										 asio::placeholders::error, asio::placeholders::bytes_transferred,
										 ssl_socket));
	}

	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent, error]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
		OnResponseSent.Broadcast(error);
	});
}

void UHttpServerSsl::read_status_line(const asio::error_code& error, const size_t bytes_recvd,
	const ssl_socket_ptr& ssl_socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, ssl_socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnSocketError.Broadcast(error, ssl_socket);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
	});
	std::istream response_stream(&*RequestBuffers[ssl_socket]);
	std::string method;
	response_stream >> method;
	std::string path;
	response_stream >> path;
	std::string version;
	response_stream >> version;

	std::string error_payload;
	std::set<std::string> versions = {"HTTP/1.0", "HTTP/1.1", "HTTP/2.0"};
	if (!ServerRequestMethod.Contains(UTF8_TO_TCHAR(method.c_str())))
	{
		if (versions.find(version) == versions.end())
		{
			version = "HTTP/1.1";
		}
		error_payload = version + " 405 Method Not Allowed\r\n";
		if (Headers.Num() > 0)
		{
			for (const TPair<FString, FString> header : Headers)
			{
				std::string key, val;
				key = TCHAR_TO_UTF8(*header.Key); val = TCHAR_TO_UTF8(*header.Value);
				error_payload += key + ": " + val + "\r\n";
			}
		}
		error_payload += "Allow: DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT, TRACE\r\n";
		error_payload += "Content-Length: 0\r\n";

		asio::async_write(
			*ssl_socket, asio::buffer(error_payload.data(), error_payload.size()),
			std::bind(&UHttpServerSsl::write_response, this, asio::placeholders::error,
			          asio::placeholders::bytes_transferred, ssl_socket, true));
		return;
	}
	if (versions.find(version) == versions.end())
	{
		error_payload = "HTTP/1.1 505 HTTP Version Not Supported\r\n";
		if (Headers.Num() > 0)
		{
			for (const TPair<FString, FString> header : Headers)
			{
				std::string key, val;
				key = TCHAR_TO_UTF8(*header.Key); val = TCHAR_TO_UTF8(*header.Value);
				error_payload += key + ": " + val + "\r\n";
			}
		}
		error_payload += "Content-Length: 0\r\n";

		asio::streambuf res_buffer;
		std::ostream request_stream(&res_buffer);
		request_stream << error_payload;
		asio::async_write(
			*ssl_socket, res_buffer,
			std::bind(&UHttpServerSsl::write_response, this, asio::placeholders::error,
			          asio::placeholders::bytes_transferred, ssl_socket, true));
		return;
	}

	FServerRequest request;
	version.erase(0, 5);
	request.Version = UTF8_TO_TCHAR(version.c_str());
	request.Method = ServerRequestMethod[UTF8_TO_TCHAR(method.c_str())];
	request.Path = UTF8_TO_TCHAR(path.c_str());

	RequestBuffers[ssl_socket]->consume(2);
	asio::async_read_until(
		*ssl_socket, *RequestBuffers[ssl_socket], "\r\n\r\n",
		std::bind(&UHttpServerSsl::read_headers, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred, request, ssl_socket));
}

void UHttpServerSsl::read_headers(const asio::error_code& error, const size_t bytes_recvd, FServerRequest& request,
	const ssl_socket_ptr& ssl_socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, ssl_socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnSocketError.Broadcast(error, ssl_socket);
		});
		return;
	}
	std::istream response_stream(RequestBuffers[ssl_socket].Get());
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
		UHttpFunctionLibrary::ServerAppendHeader(request, UTF8_TO_TCHAR(header.c_str()));

	read_body(request, ssl_socket);
}

void UHttpServerSsl::read_body(FServerRequest& request, const ssl_socket_ptr& ssl_socket)
{
	if (RequestBuffers[ssl_socket]->size() == 0) {
		FServerResponse response;
		response.Version = request.Version;
		response.Headers = Headers;
		if (request.Headers.Contains("Connection"))
			response.Headers["Connection"] = request.Headers["Connection"];
		AsyncTask(ENamedThreads::GameThread, [&, request, response, ssl_socket]()
		{
			OnRequestReceived.Broadcast(request, response, ssl_socket);
		});
		consume_request_buffer(ssl_socket);
		return;
	}

	std::ostringstream body_buffer;
	body_buffer << RequestBuffers[ssl_socket].Get();
	UHttpFunctionLibrary::ServerSetBody(request, body_buffer.str().c_str());
	
	FServerResponse response;
	response.Version = request.Version;
	response.Headers = Headers;
	if (request.Headers.Contains("Connection"))
		response.Headers["Connection"] = request.Headers["Connection"];
	AsyncTask(ENamedThreads::GameThread, [&, request, response, ssl_socket]()
	{
		OnRequestReceived.Broadcast(request, response, ssl_socket);
	});
	consume_request_buffer(ssl_socket);
}
