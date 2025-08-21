/** 
 * Copyright © 2025 Nathan Miguel
 */

#include "tcp/tcpclient.h"

void UTCPClient::BeginDestroy() {
	is_being_destroyed = true;
	if (net.socket.is_open())
		Close();
	UObject::BeginDestroy();
}

void UTCPClient::AddToRoot() {
	Super::AddToRoot();
}

void UTCPClient::RemoveFromRoot() {
	Super::RemoveFromRoot();
}

bool UTCPClient::IsRooted() {
	return Super::IsRooted();
}

void UTCPClient::MarkPendingKill() {
	Super::MarkPendingKill();
}

bool UTCPClient::IsOpen() {
	return net.socket.is_open();
}

FTcpEndpoint UTCPClient::LocalEndpoint() {
	tcp::endpoint ep = net.socket.local_endpoint();
	return FTcpEndpoint(ep);
}

FTcpEndpoint UTCPClient::RemoteEndpoint() {
	tcp::endpoint ep = net.socket.remote_endpoint();
	return FTcpEndpoint(ep);
}

FErrorCode UTCPClient::GetErrorCode() {
	return FErrorCode(error_code);
}

bool UTCPClient::Write(const FString& Message, const FDelegateTcpClientMessageSent& Callback) {
	if (!net.socket.is_open() || Message.IsEmpty())
		return false;

	asio::async_write(net.socket,
						asio::buffer(TCHAR_TO_UTF8(*Message), Message.Len()),
						[this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
							AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
								if (!is_being_destroyed)
									Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
							});
						});
	return true;
}

bool UTCPClient::WriteBuffer(const TArray<uint8>& Buffer, const FDelegateTcpClientMessageSent& Callback) {
	if (!net.socket.is_open() || !Buffer.Num())
		return false;

	asio::async_write(net.socket,
						asio::buffer(Buffer.GetData(), Buffer.Num()),
						[this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
							AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
								if (!is_being_destroyed)
									Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
							});
						});
	return true;
}

bool UTCPClient::Connect(const FClientBindOptions &BindOpts) {
	if (net.socket.is_open())
		return false;

	std::string addr = TCHAR_TO_UTF8(*BindOpts.Address);
	std::string port = TCHAR_TO_UTF8(*BindOpts.Port);
	net.resolver.async_resolve(BindOpts.Protocol == EProtocolType::V4 ? tcp::v4() : tcp::v6(),
								addr, port,
								[this](const asio::error_code &ec, const tcp::resolver::results_type &results) {
									resolve(ec, results);
								});

	asio::post(thread_pool(), [this]{ run_context_thread(); });
	return true;
}

void UTCPClient::Close() {
	is_closing.Store(true);
	if (net.socket.is_open()) {
		FScopeLock lock(&mutex_error);
		net.socket.shutdown(tcp::socket::shutdown_both, error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error_code));
			});
		net.socket.close(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error_code));
			});
	}
	net.context.stop();
	net.context.restart();
	net.endpoint = tcp::endpoint();
	if (OnError.IsBound())
		AsyncTask(ENamedThreads::GameThread, [this]() {
			if (!is_being_destroyed)
				OnClose.Broadcast();
		});
	is_closing.Store(false);
}

void UTCPClient::run_context_thread() {
	FScopeLock lock(&mutex_io);
	error_code.clear();
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UTCPClient::resolve(const asio::error_code& error, const tcp::resolver::results_type& results) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		AsyncTask(ENamedThreads::GameThread, [this, error]() {
			if (!is_being_destroyed)
				OnError.Broadcast(FErrorCode(error));
		});
		return;
	}

	net.endpoint = results.begin()->endpoint();
	asio::async_connect(net.socket,
						results,
						[this](const asio::error_code &ec, const tcp::endpoint &ep) {
							conn(ec); 
						});
}

void UTCPClient::conn(const asio::error_code& error) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		AsyncTask(ENamedThreads::GameThread, [this, error]() {
			if (!is_being_destroyed)
				OnError.Broadcast(FErrorCode(error));
		});
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [this]() {
		if (!is_being_destroyed)
			OnConnected.Broadcast();
	});
            
	consume_recv_buffer();
	asio::async_read(net.socket,
						recv_buffer, 
						asio::transfer_at_least(1),
						[this](const asio::error_code &ec, const size_t bytes_recvd) {
							read_cb(ec, bytes_recvd);
						});
}

void UTCPClient::consume_recv_buffer() {
	const size_t size = recv_buffer.size();
	if (size > 0)
		recv_buffer.consume(size);
}

void UTCPClient::read_cb(const asio::error_code& error, const size_t bytes_recvd) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		AsyncTask(ENamedThreads::GameThread, [this, error]() {
			if (!is_being_destroyed)
				OnError.Broadcast(FErrorCode(error));
		});
		return;
	}


	TArray<uint8> buffer;
	buffer.SetNum(bytes_recvd);
	asio::buffer_copy(asio::buffer(buffer.GetData(), bytes_recvd), recv_buffer.data());
	AsyncTask(ENamedThreads::GameThread, [this, buffer, bytes_recvd]() {
		if (!is_being_destroyed)
			OnMessage.Broadcast(buffer, bytes_recvd);
	});

	consume_recv_buffer();
	asio::async_read(net.socket,
						recv_buffer, 
						asio::transfer_at_least(1),
						[this](const asio::error_code &ec, const size_t bytes_received) {
							read_cb(ec, bytes_received);
						});
}

void UTCPClientSsl::BeginDestroy() {
	if (net.ssl_socket.next_layer().is_open())
		Close();
	consume_recv_buffer();
	UObject::BeginDestroy();
}

void UTCPClientSsl::AddToRoot() {
	Super::AddToRoot();
}

void UTCPClientSsl::RemoveFromRoot() {
	Super::RemoveFromRoot();
}

bool UTCPClientSsl::IsRooted() {
	return Super::IsRooted();
}

void UTCPClientSsl::MarkPendingKill() {
	Super::MarkPendingKill();
}

bool UTCPClientSsl::IsOpen() {
	return net.ssl_socket.next_layer().is_open();
}

FTcpEndpoint UTCPClientSsl::LocalEndpoint() {
	tcp::endpoint ep = net.ssl_socket.next_layer().local_endpoint();
	return FTcpEndpoint(ep);
}

FTcpEndpoint UTCPClientSsl::RemoteEndpoint() {
	tcp::endpoint ep = net.ssl_socket.next_layer().remote_endpoint();
	return FTcpEndpoint(ep);
}

FErrorCode UTCPClientSsl::GetErrorCode() {
	return FErrorCode(error_code);
}

bool UTCPClientSsl::Write(const FString& Message, const FDelegateTcpClientMessageSent& Callback) {
	if (!net.ssl_socket.next_layer().is_open() || Message.IsEmpty())
		return false;

	asio::async_write(net.ssl_socket,
						asio::buffer(TCHAR_TO_UTF8(*Message), Message.Len()),
						[this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
							AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
								if (!is_being_destroyed)
									Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
							});
						});
	return true;
}

bool UTCPClientSsl::WriteBuffer(const TArray<uint8>& Buffer, const FDelegateTcpClientMessageSent& Callback) {
	if (!net.ssl_socket.next_layer().is_open() || !Buffer.Num())
		return false;

	asio::async_write(net.ssl_socket,
						asio::buffer(Buffer.GetData(), Buffer.Num()),
						[this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
							AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
								if (!is_being_destroyed)
									Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
							});
						});
	return true;
}

bool UTCPClientSsl::Connect(const FClientBindOptions& BindOpts) {
	if (net.ssl_socket.next_layer().is_open())
		return false;

	std::string addr = TCHAR_TO_UTF8(*BindOpts.Address);
	std::string port = TCHAR_TO_UTF8(*BindOpts.Port);
	net.resolver.async_resolve(addr,
								port,
								[this](const asio::error_code &ec, const tcp::resolver::results_type &results) {
									resolve(ec, results);
								});

	asio::post(thread_pool(), [this]{ run_context_thread(); });
	return true;
}

void UTCPClientSsl::Close() {
	is_closing.Store(true);
	if (net.ssl_socket.next_layer().is_open()) {
		FScopeLock lock(&mutex_error);
		net.ssl_socket.lowest_layer().shutdown(asio::socket_base::shutdown_both, error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error_code));
			});
		net.ssl_socket.lowest_layer().close(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error_code));
			});

		net.ssl_socket.shutdown(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error_code));
			});
		net.ssl_socket.next_layer().close(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error_code));
			});
	}
	net.context.stop();
	net.context.restart();
	net.endpoint = tcp::endpoint();
	net.ssl_socket = asio::ssl::stream<tcp::socket>(net.context, net.ssl_context);
	AsyncTask(ENamedThreads::GameThread, [this]() {
		if (!is_being_destroyed)
			OnClose.Broadcast();
	});
	is_closing.Store(true);
}

void UTCPClientSsl::run_context_thread() {
	FScopeLock lock(&mutex_io);
	error_code.clear();
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UTCPClientSsl::resolve(const asio::error_code& error, const tcp::resolver::results_type& results) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this, error]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error));
			});
		return;
	}
	net.endpoint = results.begin()->endpoint();
	asio::async_connect(net.ssl_socket.lowest_layer(),
						results,
						[this](const asio::error_code &ec, const tcp::endpoint &ep) {
							conn(ec);
						});
}

void UTCPClientSsl::conn(const asio::error_code& error) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this, error]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error));
			});
		return;
	}

	net.ssl_socket.async_handshake(asio::ssl::stream_base::client,
									[this](const asio::error_code &ec) {
										ssl_handshake(ec);
									});
}

void UTCPClientSsl::ssl_handshake(const asio::error_code& error) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this, error]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error));
			});
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [this]() {
		if (!is_being_destroyed)
			OnConnected.Broadcast();
	});

	consume_recv_buffer();
	asio::async_read(net.ssl_socket,
					recv_buffer,
					asio::transfer_at_least(1),
					[this](const asio::error_code &ec, const size_t bytes_received) {
						read_cb(ec, bytes_received);
					});
}

void UTCPClientSsl::consume_recv_buffer() {
	const size_t size = recv_buffer.size();
	if (size > 0)
		recv_buffer.consume(size);
}

void UTCPClientSsl::read_cb(const asio::error_code& error, const size_t bytes_recvd) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this, error]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error));
			});
		return;
	}

	TArray<uint8> buffer;
	buffer.SetNum(bytes_recvd);
	asio::buffer_copy(asio::buffer(buffer.GetData(), bytes_recvd), recv_buffer.data());
	AsyncTask(ENamedThreads::GameThread, [this, buffer, bytes_recvd]() {
		if (!is_being_destroyed)
			OnMessage.Broadcast(buffer, bytes_recvd);
	});

	consume_recv_buffer();
	asio::async_read(net.ssl_socket,
					recv_buffer,
					asio::transfer_at_least(1),
					[this](const asio::error_code &ec, const size_t bytes_received) {
						read_cb(ec, bytes_received);
					});
}
