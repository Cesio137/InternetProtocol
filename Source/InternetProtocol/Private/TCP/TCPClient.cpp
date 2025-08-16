/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#include "tcp/tcpclient.h"

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
						asio::buffer(Message.GetCharArray().GetData(), Message.Len()),
						[&, Callback](const asio::error_code &ec, const size_t bytes_sent) {
							AsyncTask(ENamedThreads::GameThread, [&, Callback, ec, bytes_sent]() {
								Callback.Execute(FErrorCode(ec), bytes_sent);
							});
						});
	return true;
}

bool UTCPClient::WriteBuffer(const TArray<uint8>& Buffer, const FDelegateTcpClientMessageSent& Callback) {
	if (!net.socket.is_open() || !Buffer.Num())
		return false;

	asio::async_write(net.socket,
						asio::buffer(Buffer.GetData(), Buffer.Num()),
						[&, Callback](const asio::error_code &ec, const size_t bytes_sent) {
							AsyncTask(ENamedThreads::GameThread, [&, Callback, ec, bytes_sent]() {
								Callback.Execute(FErrorCode(ec), bytes_sent);
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
								[&](const asio::error_code &ec, const tcp::resolver::results_type &results) {
									resolve(ec, results);
								});

	asio::post(thread_pool(), [&]{ run_context_thread(); });
	return true;
}

void UTCPClient::Close() {
	is_closing.Store(true);
	if (net.socket.is_open()) {
		FScopeLock lock(&mutex_error);
		net.socket.shutdown(tcp::socket::shutdown_both, error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [&]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
		net.socket.close(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [&]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
	}
	net.context.stop();
	net.context.restart();
	net.endpoint = tcp::endpoint();
	AsyncTask(ENamedThreads::GameThread, [&]() {
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
		AsyncTask(ENamedThreads::GameThread, [&]() {
			OnError.Broadcast(FErrorCode(error_code));
		});
		return;
	}

	net.endpoint = results.begin()->endpoint();
	asio::async_connect(net.socket,
						results,
						[&](const asio::error_code &ec, const tcp::endpoint &ep) {
							conn(ec); 
						});
}

void UTCPClient::conn(const asio::error_code& error) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		AsyncTask(ENamedThreads::GameThread, [&]() {
			OnError.Broadcast(FErrorCode(error_code));
		});
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [&]() {
		OnConnected.Broadcast();
	});
            
	consume_recv_buffer();
	asio::async_read(net.socket,
						recv_buffer, 
						asio::transfer_at_least(1),
						[&](const asio::error_code &ec, const size_t bytes_recvd) {
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
		AsyncTask(ENamedThreads::GameThread, [&]() {
			OnError.Broadcast(FErrorCode(error_code));
		});
		return;
	}


	TArray<uint8> buffer;
	buffer.SetNum(bytes_recvd);
	asio::buffer_copy(asio::buffer(buffer.GetData(), bytes_recvd), recv_buffer.data());
	AsyncTask(ENamedThreads::GameThread, [&, buffer, bytes_recvd]() {
		OnMessage.Broadcast(buffer, bytes_recvd);
	});

	consume_recv_buffer();
	asio::async_read(net.socket,
						recv_buffer, 
						asio::transfer_at_least(1),
						[&](const asio::error_code &ec, const size_t bytes_received) {
							read_cb(ec, bytes_received);
						});
}
