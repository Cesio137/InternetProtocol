/** 
 * Copyright © 2025 Nathan Miguel
 */


#include "http/httpclient.h"
#include "utils/net.h"

bool UHttpClient::IsOpen() {
	bool is_open = net.socket.is_open();
	return is_open;
}

void UHttpClient::SetTimeout(uint8 Value) {
	idle_timeout_seconds = Value;
}

uint8 UHttpClient::GetTimeout() {
	return idle_timeout_seconds;
}

void UHttpClient::SetHost(const FClientBindOptions& BindOpts) {
	bind_options = BindOpts;
}

void UHttpClient::Request(const FHttpRequest& Req, const FDelegateHttpClientResponse& Callback) {
	if (!net.socket.is_open()) {
		net.resolver.async_resolve(bind_options.Protocol == EProtocolType::V4 ? tcp::v4() : tcp::v6(),
								   TCHAR_TO_UTF8(*bind_options.Address), TCHAR_TO_UTF8(*bind_options.Port),
								   [&, Req, Callback](const asio::error_code &ec,
														 const tcp::resolver::results_type &results) {
									   resolve(ec, results, Req, Callback);
								   });
		asio::post(thread_pool(), [&] { run_context_thread(); });
		return;
	}

	FString payload = prepare_request(Req, net.socket.remote_endpoint().address().to_string().c_str(),
										  net.socket.remote_endpoint().port());
	if (idle_timeout_seconds > 0)
		reset_idle_timer();
	asio::async_write(net.socket,
					  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
					  [&, Callback](const asio::error_code &ec, const size_t bytes_sent) {
						  write_cb(ec, bytes_sent, Callback);
					  });
}

void UHttpClient::Close() {
	is_closing.Store(true);
	asio::error_code ec;
	if (net.socket.is_open()) {
		net.socket.shutdown(tcp::socket::shutdown_both, ec);
		net.socket.close(ec);
	}
	net.context.stop();
	net.context.restart();
	net.endpoint = tcp::endpoint();
	is_closing.Store(false);
}

void UHttpClient::start_idle_timer() {
	if (idle_timeout_seconds == 0)
		return;

	idle_timer->expires_after(std::chrono::seconds(idle_timeout_seconds));
	idle_timer->async_wait([&](const asio::error_code &ec) {
		if (ec == asio::error::operation_aborted)
			return;

		if (is_closing.Load())
			return;

		Close();
	});
}

void UHttpClient::reset_idle_timer() {
	if (is_closing.Load() || idle_timeout_seconds == 0)
		return;

	idle_timer->cancel();
	start_idle_timer();
}

void UHttpClient::run_context_thread() {
	FScopeLock lock(&mutex_io);
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UHttpClient::resolve(const asio::error_code& error, const tcp::resolver::results_type& results,
	const FHttpRequest& req, const FDelegateHttpClientResponse& response_cb) {
	if (error) {
		AsyncTask(ENamedThreads::GameThread, [&, error, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(error), FHttpResponse());
		});
		return;
	}

	net.endpoint = results.begin()->endpoint();
	asio::async_connect(net.socket,
						results,
						[&, req, response_cb](const asio::error_code &ec, const tcp::endpoint &ep) {
							conn(ec, req, response_cb);
						});
}

void UHttpClient::conn(const asio::error_code& error, const FHttpRequest& req,
	const FDelegateHttpClientResponse& response_cb) {
	if (error) {
		AsyncTask(ENamedThreads::GameThread, [&, error, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(error), FHttpResponse());
		});
		return;
	}

	FString payload = prepare_request(req, net.socket.remote_endpoint().address().to_string().c_str(),
										  net.socket.remote_endpoint().port());
	if (idle_timeout_seconds > 0)
		start_idle_timer();
	asio::async_write(net.socket,
					  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
					  [&, response_cb](const asio::error_code &ec, const size_t bytes_sent) {
						  write_cb(ec, bytes_sent, response_cb);
					  });
}

void UHttpClient::write_cb(const asio::error_code& error, const size_t bytes_sent,
	const FDelegateHttpClientResponse& response_cb) {
	if (error) {
		AsyncTask(ENamedThreads::GameThread, [&, error, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(error), FHttpResponse());
		});
		return;
	}
	
	consume_recv_buffer();
	if (idle_timeout_seconds > 0)
		reset_idle_timer();
	asio::async_read_until(net.socket,
						   recv_buffer, "\r\n",
						   [&, response_cb](const asio::error_code &ec, const size_t bytes_received) {
							   read_cb(ec, bytes_received, response_cb);
						   });
}

void UHttpClient::consume_recv_buffer() {
	const size_t size = recv_buffer.size();
	if (size > 0)
		recv_buffer.consume(size);
}

void UHttpClient::read_cb(const asio::error_code& error, const size_t bytes_recvd,
	const FDelegateHttpClientResponse& response_cb) {
	if (error) {
		consume_recv_buffer();
		AsyncTask(ENamedThreads::GameThread, [&, error, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(error), FHttpResponse());
		});
		return;
	}
	if (idle_timeout_seconds > 0)
		reset_idle_timer();

	FHttpResponse response;
	std::istream response_stream(&recv_buffer);
	std::string http_version;
	unsigned int status_code;
	std::string status_message;
	response_stream >> http_version;
	response_stream >> status_code;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
		consume_recv_buffer();
		response.Status_Code = 505;
		response.Status_Message = "HTTP Version Not Supported";
		AsyncTask(ENamedThreads::GameThread, [&, response, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(), response);
		});
		return;
	}
	response.Status_Code = status_code;
	response.Status_Message = status_message.c_str();
	if (status_code != 200 && recv_buffer.size() == 0) {
		consume_recv_buffer();
		AsyncTask(ENamedThreads::GameThread, [&, response, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(), response);
		});
		return;
	}

	asio::async_read_until(net.socket,
						   recv_buffer, "\r\n\r\n",
						   std::bind(&UHttpClient::read_headers,
									 this, asio::placeholders::error,
									 response,
									 response_cb));
}

void UHttpClient::read_headers(const asio::error_code& error, FHttpResponse& response,
	const FDelegateHttpClientResponse& response_cb) {
	if (error) {
		consume_recv_buffer();
		AsyncTask(ENamedThreads::GameThread, [&, error, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(error), FHttpResponse());
		});
		return;
	}
	std::istream response_stream(&recv_buffer);
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
		res_append_header(response, header);

	if (recv_buffer.size() > 0) {
		std::ostringstream body_buffer;
		body_buffer << &recv_buffer;
		response.Body = body_buffer.str().c_str();
	}

	consume_recv_buffer();
	AsyncTask(ENamedThreads::GameThread, [&, response, response_cb]() {
		response_cb.ExecuteIfBound(FErrorCode(), response);
	});
}

bool UHttpClientSsl::IsOpen() {
	return net.ssl_socket.next_layer().is_open();
}

void UHttpClientSsl::SetTimeout(uint8 Value) {
	idle_timeout_seconds = Value;
}

uint8 UHttpClientSsl::GetTimeout() {
	return idle_timeout_seconds;
}

void UHttpClientSsl::SetHost(const FClientBindOptions& BindOpts) {
	bind_options = BindOpts;
}

void UHttpClientSsl::Request(const FHttpRequest& Request, const FDelegateHttpClientResponse& Callback) {
	if (!net.ssl_socket.next_layer().is_open()) {
		net.resolver.async_resolve(bind_options.Protocol == EProtocolType::V4 ? tcp::v4() : tcp::v6(),
								   TCHAR_TO_UTF8(*bind_options.Address), TCHAR_TO_UTF8(*bind_options.Port),
								   [&, Request, Callback](const asio::error_code &ec,
														 const tcp::resolver::results_type &results) {
									   resolve(ec, results, Request, Callback);
								   });
		asio::post(thread_pool(), [&] { run_context_thread(); });
		return;
	}

	FString payload = prepare_request(Request, net.ssl_socket.next_layer().remote_endpoint().address().to_string().c_str(),
										  net.ssl_socket.next_layer().remote_endpoint().port());
	if (idle_timeout_seconds > 0)
		reset_idle_timer();
	asio::async_write(net.ssl_socket,
					  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
					  [&, Callback](const asio::error_code &ec, const size_t bytes_sent) {
						  write_cb(ec, bytes_sent, Callback);
					  });
}

void UHttpClientSsl::Close() {
	is_closing.Store(true);
	asio::error_code ec;
	if (net.ssl_socket.next_layer().is_open()) {
		net.ssl_socket.lowest_layer().shutdown(asio::socket_base::shutdown_both, ec);
		net.ssl_socket.lowest_layer().close(ec);

		net.ssl_socket.shutdown(ec);
		net.ssl_socket.next_layer().close(ec);
	}
	net.context.stop();
	net.context.restart();
	net.endpoint = tcp::endpoint();
	is_closing.Store(false);
}

void UHttpClientSsl::start_idle_timer() {
	if (idle_timeout_seconds == 0)
		return;

	idle_timer->expires_after(std::chrono::seconds(idle_timeout_seconds));
	idle_timer->async_wait([&](const asio::error_code &ec) {
		if (ec == asio::error::operation_aborted)
			return;

		if (is_closing.Load())
			return;

		Close();
	});
}

void UHttpClientSsl::reset_idle_timer() {
	if (is_closing.Load() || idle_timeout_seconds == 0)
		return;

	idle_timer->cancel();
	start_idle_timer();
}

void UHttpClientSsl::run_context_thread() {
	FScopeLock lock(&mutex_io);
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UHttpClientSsl::resolve(const asio::error_code& error, const tcp::resolver::results_type& results,
	const FHttpRequest& req, const FDelegateHttpClientResponse& response_cb) {
	if (error) {
		AsyncTask(ENamedThreads::GameThread, [&, error, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(error), FHttpResponse());
		});
		return;
	}

	net.endpoint = results.begin()->endpoint();
	asio::async_connect(net.ssl_socket.lowest_layer(),
						results,
						[&, req, response_cb](const asio::error_code &ec, const tcp::endpoint &ep) {
							conn(ec, req, response_cb);
						});
}

void UHttpClientSsl::conn(const asio::error_code& error, const FHttpRequest& req,
	const FDelegateHttpClientResponse& response_cb) {
	if (error) {
		AsyncTask(ENamedThreads::GameThread, [&, error, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(error), FHttpResponse());
		});
		return;
	}

	net.ssl_socket.async_handshake(asio::ssl::stream_base::client,
								   [&, req, response_cb](const asio::error_code &ec) {
									   ssl_handshake(ec, req, response_cb);
								   });
}

void UHttpClientSsl::ssl_handshake(const asio::error_code& error, const FHttpRequest& req,
	const FDelegateHttpClientResponse& response_cb) {
	if (error) {
		AsyncTask(ENamedThreads::GameThread, [&, error, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(error), FHttpResponse());
		});
		return;
	}

	FString payload = prepare_request(req, net.ssl_socket.next_layer().remote_endpoint().address().to_string().c_str(),
										  net.ssl_socket.next_layer().remote_endpoint().port());
	if (idle_timeout_seconds > 0)
		start_idle_timer();
	asio::async_write(net.ssl_socket,
					  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
					  [&, response_cb](const asio::error_code &ec, const size_t bytes_sent) {
						  write_cb(ec, bytes_sent, response_cb);
					  });
}

void UHttpClientSsl::write_cb(const asio::error_code& error, const size_t bytes_sent,
	const FDelegateHttpClientResponse& response_cb) {
	if (error) {
		AsyncTask(ENamedThreads::GameThread, [&, error, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(error), FHttpResponse());
		});
		return;
	}
	
	consume_recv_buffer();
	if (idle_timeout_seconds > 0)
		reset_idle_timer();
	asio::async_read_until(net.ssl_socket,
						   recv_buffer, "\r\n",
						   [&, response_cb](const asio::error_code &ec, const size_t bytes_received) {
							   read_cb(ec, bytes_received, response_cb);
						   });
}

void UHttpClientSsl::consume_recv_buffer() {
	const size_t size = recv_buffer.size();
	if (size > 0)
		recv_buffer.consume(size);
}

void UHttpClientSsl::read_cb(const asio::error_code& error, const size_t bytes_recvd,
	const FDelegateHttpClientResponse& response_cb) {
	if (error) {
		consume_recv_buffer();
		AsyncTask(ENamedThreads::GameThread, [&, error, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(error), FHttpResponse());
		});
		return;
	}
	if (idle_timeout_seconds > 0)
		reset_idle_timer();

	FHttpResponse response;
	std::istream response_stream(&recv_buffer);
	std::string http_version;
	unsigned int status_code;
	std::string status_message;
	response_stream >> http_version;
	response_stream >> status_code;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
		consume_recv_buffer();
		response.Status_Code = 505;
		response.Status_Message = "HTTP Version Not Supported";
		AsyncTask(ENamedThreads::GameThread, [&, response, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(), response);
		});
		return;
	}
	response.Status_Code = status_code;
	response.Status_Message = status_message.c_str();
	if (status_code != 200 && recv_buffer.size() == 0) {
		consume_recv_buffer();
		AsyncTask(ENamedThreads::GameThread, [&, response, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(), response);
		});
		return;
	}

	asio::async_read_until(net.ssl_socket,
						   recv_buffer, "\r\n\r\n",
						   std::bind(&UHttpClientSsl::read_headers,
									 this, asio::placeholders::error,
									 response,
									 response_cb));
}

void UHttpClientSsl::read_headers(const asio::error_code& error, FHttpResponse& response,
	const FDelegateHttpClientResponse& response_cb) {
	if (error) {
		consume_recv_buffer();
		AsyncTask(ENamedThreads::GameThread, [&, error, response_cb]() {
			response_cb.ExecuteIfBound(FErrorCode(error), FHttpResponse());
		});
		return;
	}
	std::istream response_stream(&recv_buffer);
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
		res_append_header(response, header);

	if (recv_buffer.size() > 0) {
		std::ostringstream body_buffer;
		body_buffer << &recv_buffer;
		response.Body = body_buffer.str().c_str();
	}

	consume_recv_buffer();
	AsyncTask(ENamedThreads::GameThread, [&, response, response_cb]() {
		response_cb.ExecuteIfBound(FErrorCode(), response);
	});
}
