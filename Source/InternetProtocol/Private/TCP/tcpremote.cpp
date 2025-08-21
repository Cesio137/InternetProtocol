/** 
 * Copyright © 2025 Nathan Miguel
 */

#include "tcp/tcpremote.h"

void UTCPRemote::Construct(TSharedPtr<tcp::socket>& socket_ptr) {
	if (!IsRooted()) AddToRoot();
	socket = socket_ptr;
}

void UTCPRemote::Destroy() {
	if (IsRooted()) {
		RemoveFromRoot();
	}

	MarkPendingKill();
}

bool UTCPRemote::IsOpen() {
	return socket->is_open();
}

FTcpEndpoint UTCPRemote::LocalEndpoint() {
	tcp::endpoint ep = socket->local_endpoint();
	return FTcpEndpoint(ep);
}

FTcpEndpoint UTCPRemote::RemoteEndpoint() {
	tcp::endpoint ep = socket->remote_endpoint();
	return FTcpEndpoint(ep);
}

tcp::socket& UTCPRemote::get_socket() {
	return *socket;
}

FErrorCode UTCPRemote::GetErrorCode() {
	return FErrorCode(error_code);
}

bool UTCPRemote::Write(const FString& Message, const FDelegateTcpRemoteMessageSent& Callback) {
	if (!socket->is_open() || Message.IsEmpty())
		return false;

	asio::async_write(*socket,
					  asio::buffer(TCHAR_TO_UTF8(*Message), Message.Len()),
					  [this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
					  		AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
								Callback.Execute(FErrorCode(ec), bytes_sent);
							});
					  });
	return true;
}

bool UTCPRemote::WriteBuffer(const TArray<uint8>& Buffer, const FDelegateTcpRemoteMessageSent& Callback) {
	if (!socket->is_open() || !Buffer.Num())
		return false;

	asio::async_write(*socket,
						asio::buffer(Buffer.GetData(), Buffer.Num()),
						[this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
							AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
								Callback.Execute(FErrorCode(ec), bytes_sent);
							});
						});
	return true;
}

void UTCPRemote::connect() {
	asio::async_read(*socket,
						recv_buffer,
						asio::transfer_at_least(1),
						[this](const asio::error_code &ec, const size_t bytes_received) {
							read_cb(ec, bytes_received);
						});
}

void UTCPRemote::Close() {
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
}

void UTCPRemote::consume_recv_buffer() {
	const size_t size = recv_buffer.size();
	if (size > 0)
		recv_buffer.consume(size);
}

void UTCPRemote::read_cb(const asio::error_code& error, std::size_t bytes_recvd) {
	if (error) {
		AsyncTask(ENamedThreads::GameThread, [this, error]() {
			OnError.Broadcast(FErrorCode(error));
			OnClose.Broadcast();
			if (on_close) on_close();
		});
		return;
	}

	TArray<uint8> buffer;
	buffer.SetNum(bytes_recvd);
	asio::buffer_copy(asio::buffer(buffer.GetData(), bytes_recvd), recv_buffer.data());
	AsyncTask(ENamedThreads::GameThread, [this, buffer, bytes_recvd]() {
		OnMessage.Broadcast(buffer, bytes_recvd);
	});

	consume_recv_buffer();
	asio::async_read(*socket,
					 recv_buffer,
					 asio::transfer_at_least(1),
					 [this](const asio::error_code &ec, const size_t bytes_received) {
						 read_cb(ec, bytes_received);
					 });
}

void UTCPRemoteSsl::Construct(TSharedPtr<asio::ssl::stream<tcp::socket>>& socket_ptr) {
	if (!IsRooted()) AddToRoot();
	ssl_socket = socket_ptr;
}

void UTCPRemoteSsl::Destroy() {
	if (IsRooted()) {
		RemoveFromRoot();
	}

	MarkPendingKill();
}

bool UTCPRemoteSsl::IsOpen() {
	return  ssl_socket->next_layer().is_open();
}

FTcpEndpoint UTCPRemoteSsl::LocalEndpoint() {
	tcp::endpoint ep = ssl_socket->next_layer().local_endpoint();
	return FTcpEndpoint(ep);
}

FTcpEndpoint UTCPRemoteSsl::RemoteEndpoint() {
	tcp::endpoint ep = ssl_socket->next_layer().remote_endpoint();
	return FTcpEndpoint(ep);
}

asio::ssl::stream<tcp::socket>& UTCPRemoteSsl::get_socket() {
	return *ssl_socket;
}

FErrorCode UTCPRemoteSsl::GetErrorCode() {
	return FErrorCode(error_code);
}

bool UTCPRemoteSsl::Write(const FString& Message, const FDelegateTcpRemoteMessageSent& Callback) {
	if (!ssl_socket->next_layer().is_open() || Message.IsEmpty())
		return false;

	asio::async_write(*ssl_socket,
						asio::buffer(TCHAR_TO_UTF8(*Message), Message.Len()),
						[this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
							AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
								Callback.Execute(FErrorCode(ec), bytes_sent);
							});
						});
	return true;
}

bool UTCPRemoteSsl::WriteBuffer(const TArray<uint8>& Buffer, const FDelegateTcpRemoteMessageSent& Callback) {
	if (!ssl_socket->next_layer().is_open() || !Buffer.Num())
		return false;

	asio::async_write(*ssl_socket,
						asio::buffer(Buffer.GetData(), Buffer.Num()),
						[this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
							AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
								Callback.Execute(FErrorCode(ec), bytes_sent);
							});
						});
	return true;
}

void UTCPRemoteSsl::connect() {
	ssl_socket->async_handshake(asio::ssl::stream_base::server,
									[this](const asio::error_code &ec) {
										ssl_handshake(ec);
									});
}

void UTCPRemoteSsl::Close() {
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
}

void UTCPRemoteSsl::ssl_handshake(const asio::error_code& error) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		AsyncTask(ENamedThreads::GameThread, [this, error]() {
			OnError.Broadcast(FErrorCode(error));
		});
		AsyncTask(ENamedThreads::GameThread, [this]() {
			OnClose.Broadcast();
			if (on_close) on_close();
		});
		return;
	}

	consume_recv_buffer();
	asio::async_read(*ssl_socket,
					 recv_buffer,
					 asio::transfer_at_least(1),
					 [this](const asio::error_code &ec, const size_t bytes_received) {
						 read_cb(ec, bytes_received);
					 });
}

void UTCPRemoteSsl::consume_recv_buffer() {
	const size_t size = recv_buffer.size();
	if (size > 0)
		recv_buffer.consume(size);
}

void UTCPRemoteSsl::read_cb(const asio::error_code& error, std::size_t bytes_recvd) {
	if (error) {
		AsyncTask(ENamedThreads::GameThread, [this, error]() {
			OnError.Broadcast(FErrorCode(error));
			OnClose.Broadcast();
			if (on_close) on_close();
		});
		return;
	}

	TArray<uint8> buffer;
	buffer.SetNum(bytes_recvd);
	asio::buffer_copy(asio::buffer(buffer.GetData(), bytes_recvd), recv_buffer.data());
	AsyncTask(ENamedThreads::GameThread, [this, buffer, bytes_recvd]() {
		OnMessage.Broadcast(buffer, bytes_recvd);
	});

	consume_recv_buffer();
	asio::async_read(*ssl_socket,
					 recv_buffer,
					 asio::transfer_at_least(1),
					 [this](const asio::error_code &ec, const size_t bytes_received) {
						 read_cb(ec, bytes_received);
					 });
}
