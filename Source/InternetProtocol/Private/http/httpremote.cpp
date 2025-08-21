/** 
 * Copyright © 2025 Nathan Miguel
 */

#include "http/httpremote.h"
#include "utils/net.h"

void UHttpRemote::Construct(asio::io_context &io_context, TSharedPtr<tcp::socket>& sock, const uint8 timeout) {
	if (!IsRooted()) AddToRoot();
	if (timeout > 0) {
		idle_timeout_seconds = timeout;
		idle_timer = MakeUnique<asio::steady_timer>(io_context, asio::steady_timer::duration(timeout));
	}
	socket = sock;
}

void UHttpRemote::Destroy() {
	if (IsRooted()) {
		RemoveFromRoot();
	}
    
	MarkPendingKill();
}

bool UHttpRemote::IsOpen() {
	return socket->is_open();
}

FTcpEndpoint UHttpRemote::LocalEndpoint() {
	tcp::endpoint ep = socket->local_endpoint();
	return FTcpEndpoint(ep);
}

FTcpEndpoint UHttpRemote::RemoteEndpoint() {
	tcp::endpoint ep = socket->remote_endpoint();
	return FTcpEndpoint(ep);
}

FErrorCode UHttpRemote::GetErrorCode() {
	return FErrorCode(error_code);
}

tcp::socket& UHttpRemote::get_socket() {
	return *socket;
}

void UHttpRemote::Headers(const FHttpResponse& Response) {
	response = Response;
}

bool UHttpRemote::Write(const FDelegateHttpRemoteMessageSent& Callback) {
	if (!socket->is_open())
		return false;

	reset_idle_timer();
	
	FString payload = prepare_response(response);
	asio::async_write(*socket,
					  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
					  [this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
						  write_cb(ec, bytes_sent, Callback);
					  });
	return true;
}

bool UHttpRemote::write() {
	if (!socket->is_open())
		return false;

	reset_idle_timer();
	FString payload = prepare_response(response);
	asio::async_write(*socket,
					  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
					  [this](const asio::error_code &ec, const size_t bytes_sent) {
						  write_error_cb(ec, bytes_sent);
					  });
	return true;
}

void UHttpRemote::connect() {
	start_idle_timer();
	asio::async_read_until(*socket, recv_buffer, "\r\n",
						   [this](const asio::error_code &ec, const size_t bytes_received) {
							   read_cb(ec, bytes_received);
						   });
}

void UHttpRemote::Close() {
	is_closing.Store(true);
	if (socket->is_open()) {
		FScopeLock lock(&mutex_error);
		socket->shutdown(tcp::socket::shutdown_both, error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
		socket->close(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
	}
	is_closing.Store(false);
}

void UHttpRemote::start_idle_timer() {
	if (idle_timeout_seconds == 0)
		return;

	idle_timer->expires_after(std::chrono::seconds(idle_timeout_seconds));
	idle_timer->async_wait([this](const asio::error_code &ec) {
		if (ec == asio::error::operation_aborted)
			return;

		if (is_closing.Load())
			return;

		Close();
		AsyncTask(ENamedThreads::GameThread, [this]() {
			OnClose.Broadcast();
			if (on_close) on_close();
		});
	});
}

void UHttpRemote::reset_idle_timer() {
	if (is_closing.Load() || idle_timeout_seconds == 0)
		return;

	idle_timer->cancel();
	start_idle_timer();
}

void UHttpRemote::write_cb(const asio::error_code& error, const size_t bytes_sent,
	const FDelegateHttpRemoteMessageSent& callback) {
	if (!will_close)
		reset_idle_timer();
	AsyncTask(ENamedThreads::GameThread, [this, error, bytes_sent, callback]() {
		callback.ExecuteIfBound(FErrorCode(error), bytes_sent);
	});
	if (will_close) {
		if (idle_timeout_seconds != 0)
			idle_timer->cancel();
		Close();
		AsyncTask(ENamedThreads::GameThread, [this]() {
			OnClose.Broadcast();
			if (on_close) on_close();
		});
	}
}

void UHttpRemote::write_error_cb(const asio::error_code& error, const size_t bytes_sent) {
	if (!will_close)
		reset_idle_timer();
	AsyncTask(ENamedThreads::GameThread, [this, error]() {
		OnError.Broadcast(FErrorCode(error));
	});
	if (will_close) {
		if (idle_timeout_seconds != 0)
			idle_timer->cancel();
		Close();
		AsyncTask(ENamedThreads::GameThread, [this]() {
			OnClose.Broadcast();
			if (on_close) on_close();
		});
	}
}

void UHttpRemote::consume_recv_buffer() {
	const size_t size = recv_buffer.size();
	if (size > 0)
		recv_buffer.consume(size);
}

void UHttpRemote::read_cb(const asio::error_code& error, const size_t bytes_recvd) {
	if (error) {
		consume_recv_buffer();
		if (error != asio::error::eof)
			AsyncTask(ENamedThreads::GameThread, [this, error]() {
				OnError.Broadcast(FErrorCode(error));
			});
		if (!is_closing.Load()) {
			if (idle_timeout_seconds != 0)
				idle_timer->cancel();
			Close();
		}
		AsyncTask(ENamedThreads::GameThread, [this]() {
			OnClose.Broadcast();
			if (on_close) on_close();
		});
		return;
	}
	reset_idle_timer();

	std::istream request_stream(&recv_buffer);
	std::string method;
	std::string path;
	std::string version;

	request_stream >> method;
	request_stream >> path;
	request_stream >> version;

	if (version != "HTTP/1.0" && version != "HTTP/1.1" && version != "HTTP/2.0") {
		response.Status_Code = 400;
		response.Status_Message = "Bad Request";
		response.Body = "HTTP version not supported.";
		response.Headers.Add("Content-Type", "text/plain");
		response.Headers.Add("Content-Length", FString::FromInt(response.Body.Len()));
		will_close = true;
		write();
		return;
	}

	if (string_to_request_method(method.c_str()) == ERequestMethod::UNKNOWN) {
		response.Body = "Method not supported.";
		response.Headers.Add("Allow", "DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT, TRACE");
		response.Headers.Add("Content-Type", "text/plain");
		response.Headers.Add("Content-Length", FString::FromInt(response.Body.Len()));
		response.Status_Code = 400;
		response.Status_Message = "Bad Request";
		will_close = true;
		write();
		return;
	}

	version.erase(0, 5);
	recv_buffer.consume(2);
	asio::async_read_until(*socket,
						   recv_buffer, "\r\n\r\n",
						   [this, path, version](const asio::error_code &ec, const size_t bytes_received) {
							   read_headers(ec, path, version);
						   });
}

void UHttpRemote::read_headers(const asio::error_code& error, const std::string& path, const std::string& version) {
	if (error) {
		consume_recv_buffer();
		if (error != asio::error::eof)
			AsyncTask(ENamedThreads::GameThread, [this, error]() {
				OnError.Broadcast(FErrorCode(error));
			});
		if (!is_closing.Load()) {
			idle_timer->cancel();
			Close();
		}
		AsyncTask(ENamedThreads::GameThread, [this]() {
			OnClose.Broadcast();
			if (on_close) on_close();
		});
		return;
	}
	FHttpRequest req;
	req.Version = version.c_str();
	req.Path = path.c_str();

	std::istream request_stream(&recv_buffer);
	std::string header;

	while (std::getline(request_stream, header) && header != "\r")
		req_append_header(req, header);

	if (recv_buffer.size() > 0) {
		std::ostringstream body_buffer;
		body_buffer << &recv_buffer;
		req.Body = body_buffer.str().c_str();
	}

	response.Status_Code = 200;
	response.Status_Message = "OK";
	response.Headers.Add("Content-Type", "text/plain");
	response.Headers.Add("X-Powered-By", "ASIO");

	will_close = req.Headers.Contains("Connection") ? req.Headers["Connection"] != "keep-alive" : true;
	if (!will_close)
		will_close = req.Headers.Contains("connection") ? req.Headers["connection"] != "keep-alive" : true;

	consume_recv_buffer();
	if (on_request) on_request(req);
}

void UHttpRemoteSsl::Construct(TSharedPtr<asio::ssl::stream<tcp::socket>>& socket_ptr, asio::io_context &io_context, const uint8 timeout) {
	if (!IsRooted()) AddToRoot();
	if (timeout > 0) {
		idle_timeout_seconds = timeout;
		idle_timer = MakeUnique<asio::steady_timer>(io_context, asio::steady_timer::duration(timeout));
	}
	ssl_socket = socket_ptr;
}

void UHttpRemoteSsl::Destroy() {
	if (IsRooted()) {
		RemoveFromRoot();
	}
    
	MarkPendingKill();
}

bool UHttpRemoteSsl::IsOpen() {
	return ssl_socket->next_layer().is_open();
}

FTcpEndpoint UHttpRemoteSsl::LocalEndpoint() {
	tcp::endpoint ep = ssl_socket->next_layer().local_endpoint();
	return FTcpEndpoint(ep);
}

FTcpEndpoint UHttpRemoteSsl::RemoteEndpoint() {
	tcp::endpoint ep = ssl_socket->next_layer().remote_endpoint();
	return FTcpEndpoint(ep);
}

FErrorCode UHttpRemoteSsl::GetErrorCode() {
	return FErrorCode(error_code);
}

asio::ssl::stream<tcp::socket>& UHttpRemoteSsl::get_socket() {
	return *ssl_socket;
}

void UHttpRemoteSsl::Headers(const FHttpResponse& Response) {
	response = Response;
}

bool UHttpRemoteSsl::Write(const FDelegateHttpRemoteMessageSent& Callback) {
	if (!ssl_socket->next_layer().is_open())
		return false;

	reset_idle_timer();

	FString payload = prepare_response(response);
	asio::async_write(*ssl_socket,
					  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
					  [this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
						  write_cb(ec, bytes_sent, Callback);
					  });
	return true;
}

bool UHttpRemoteSsl::write() {
	if (!ssl_socket->next_layer().is_open())
		return false;

	reset_idle_timer();
	FString payload = prepare_response(response);
	asio::async_write(*ssl_socket,
					  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
					  [this](const asio::error_code &ec, const size_t bytes_sent) {
						  write_error_cb(ec, bytes_sent);
					  });
	return true;
}

void UHttpRemoteSsl::connect() {
	start_idle_timer();
	ssl_socket->async_handshake(asio::ssl::stream_base::server,
								 [this](const asio::error_code &ec) {
									 ssl_handshake(ec);
								 });
}

void UHttpRemoteSsl::Close() {
	is_closing.Store(true);
	if (ssl_socket->next_layer().is_open()) {
		FScopeLock lock(&mutex_error);
		ssl_socket->lowest_layer().shutdown(asio::socket_base::shutdown_both, error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
		ssl_socket->lowest_layer().close(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});

		ssl_socket->shutdown(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
		ssl_socket->next_layer().close(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
	}
	is_closing.Store(false);
}

void UHttpRemoteSsl::start_idle_timer() {
	if (idle_timeout_seconds == 0)
		return;

	idle_timer->expires_after(std::chrono::seconds(idle_timeout_seconds));
	idle_timer->async_wait([this](const asio::error_code &ec) {
		if (ec == asio::error::operation_aborted)
			return;

		if (is_closing.Load())
			return;

		Close();
		AsyncTask(ENamedThreads::GameThread, [this]() {
			OnClose.Broadcast();
			if (on_close) on_close();
		});
	});
}

void UHttpRemoteSsl::reset_idle_timer() {
	if (is_closing.Load() || idle_timeout_seconds == 0)
		return;

	idle_timer->cancel();
	start_idle_timer();
}

void UHttpRemoteSsl::ssl_handshake(const asio::error_code& error) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		AsyncTask(ENamedThreads::GameThread, [this, error]() {
			OnError.Broadcast(FErrorCode(error));
			OnClose.Broadcast();
			if (on_close) on_close();
		});
		return;
	}

	asio::async_read_until(*ssl_socket, recv_buffer, "\r\n",
						   [this](const asio::error_code &ec, const size_t bytes_received) {
							   read_cb(ec, bytes_received);
						   });
}

void UHttpRemoteSsl::write_cb(const asio::error_code& error, const size_t bytes_sent,
	const FDelegateHttpRemoteMessageSent& callback) {
	if (!will_close)
		reset_idle_timer();
	AsyncTask(ENamedThreads::GameThread, [this, error, bytes_sent, callback]() {
		callback.ExecuteIfBound(FErrorCode(error), bytes_sent);
	});
	if (will_close) {
		if (idle_timeout_seconds != 0)
			idle_timer->cancel();
		Close();
		AsyncTask(ENamedThreads::GameThread, [this]() {
			OnClose.Broadcast();
			if (on_close) on_close();
		});
	}
}

void UHttpRemoteSsl::write_error_cb(const asio::error_code& error, const size_t bytes_sent) {
	if (!will_close)
		reset_idle_timer();
	AsyncTask(ENamedThreads::GameThread, [this, error]() {
		OnError.Broadcast(FErrorCode(error));
	});
	if (will_close) {
		if (idle_timeout_seconds != 0)
			idle_timer->cancel();
		Close();
		AsyncTask(ENamedThreads::GameThread, [this]() {
			OnClose.Broadcast();
			if (on_close) on_close();
		});
	}
}

void UHttpRemoteSsl::consume_recv_buffer() {
	const size_t size = recv_buffer.size();
	if (size > 0)
		recv_buffer.consume(size);
}

void UHttpRemoteSsl::read_cb(const asio::error_code& error, const size_t bytes_recvd) {
	if (error) {
		consume_recv_buffer();
		if (error != asio::error::eof)
			AsyncTask(ENamedThreads::GameThread, [this, error]() {
				OnError.Broadcast(FErrorCode(error));
			});
		if (!is_closing.Load()) {
			if (idle_timeout_seconds != 0)
				idle_timer->cancel();
			Close();
		}
		AsyncTask(ENamedThreads::GameThread, [this]() {
			OnClose.Broadcast();
			if (on_close) on_close();
		});
		return;
	}
	reset_idle_timer();

	std::istream request_stream(&recv_buffer);
	std::string method;
	std::string path;
	std::string version;

	request_stream >> method;
	request_stream >> path;
	request_stream >> version;

	if (version != "HTTP/1.0" && version != "HTTP/1.1" && version != "HTTP/2.0") {
		response.Status_Code = 400;
		response.Status_Message = "Bad Request";
		response.Body = "HTTP version not supported.";
		response.Headers.Add("Content-Type", "text/plain");
		response.Headers.Add("Content-Length", FString::FromInt(response.Body.Len()));
		will_close = true;
		write();
		return;
	}

	if (string_to_request_method(method.c_str()) == ERequestMethod::UNKNOWN) {
		response.Body = "Method not supported.";
		response.Headers.Add("Allow", "DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT, TRACE");
		response.Headers.Add("Content-Type", "text/plain");
		response.Headers.Add("Content-Length", FString::FromInt(response.Body.Len()));
		response.Status_Code = 400;
		response.Status_Message = "Bad Request";
		will_close = true;
		write();
		return;
	}

	version.erase(0, 5);
	recv_buffer.consume(2);
	asio::async_read_until(*ssl_socket,
						   recv_buffer, "\r\n\r\n",
						   [this, path, version](const asio::error_code &ec, const size_t bytes_received) {
							   read_headers(ec, path, version);
						   });
}

void UHttpRemoteSsl::read_headers(const asio::error_code& error, const std::string& path, const std::string& version) {
	if (error) {
		consume_recv_buffer();
		if (error != asio::error::eof)
			AsyncTask(ENamedThreads::GameThread, [this, error]() {
				OnError.Broadcast(FErrorCode(error));
			});
		if (!is_closing.Load()) {
			idle_timer->cancel();
			Close();
		}
		AsyncTask(ENamedThreads::GameThread, [this]() {
			OnClose.Broadcast();
			if (on_close) on_close();
		});
		return;
	}
	FHttpRequest req;
	req.Version = version.c_str();
	req.Path = path.c_str();

	std::istream request_stream(&recv_buffer);
	std::string header;

	while (std::getline(request_stream, header) && header != "\r")
		req_append_header(req, header);

	if (recv_buffer.size() > 0) {
		std::ostringstream body_buffer;
		body_buffer << &recv_buffer;
		req.Body = body_buffer.str().c_str();
	}

	response.Status_Code = 200;
	response.Status_Message = "OK";
	response.Headers.Add("Content-Type", "text/plain");
	response.Headers.Add("X-Powered-By", "ASIO");

	will_close = req.Headers.Contains("Connection") ? req.Headers["Connection"] != "keep-alive" : true;
	if (!will_close)
		will_close = req.Headers.Contains("connection") ? req.Headers["connection"] != "keep-alive" : true;

	consume_recv_buffer();
	if (on_request) on_request(req);
}
