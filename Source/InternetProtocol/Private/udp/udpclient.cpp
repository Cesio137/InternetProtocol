/** 
 * Copyright © 2025 Nathan Miguel
 */

#include "udp/udpclient.h"

void UUDPClient::BeginDestroy() {
	is_being_destroyed = true;
	net.resolver.cancel();
	if (net.socket.is_open())
		Close();
	UObject::BeginDestroy();
}

void UUDPClient::AddToRoot() {
	Super::AddToRoot();
}

void UUDPClient::RemoveFromRoot() {
	Super::RemoveFromRoot();
}

bool UUDPClient::IsRooted() {
	return Super::IsRooted();
}

bool UUDPClient::IsOpen() {
	return net.socket.is_open();
}

FUdpEndpoint UUDPClient::LocalEndpoint() {
	udp::endpoint ep = net.socket.local_endpoint();
	return FUdpEndpoint(ep);
}

FUdpEndpoint UUDPClient::RemoteEndpoint() {
	udp::endpoint ep = net.socket.remote_endpoint();
	return FUdpEndpoint(ep);
}

void UUDPClient::SetRecvBufferSize(int Val) {
	recv_buffer_size = Val < 0 ? 0 : Val;
}

int UUDPClient::GetRecvBufferSize() {
	return recv_buffer_size;
}

FErrorCode UUDPClient::GetErrorCode() {
	return FErrorCode(error_code);
}

bool UUDPClient::Send(const FString& Message, const FDelegateUdpClientMessageSent& Callback) {
	if (!net.socket.is_open() || Message.IsEmpty())
		return false;

	net.socket.async_send_to(asio::buffer(TCHAR_TO_UTF8(*Message), Message.Len()),
								net.endpoint,
								[this, Callback](const asio::error_code& ec, std::size_t bytes_sent) {
									AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
										if (!is_being_destroyed)
											Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
									});
								});
	return true;
}

bool UUDPClient::SendBuffer(const TArray<uint8>& Buffer, const FDelegateUdpClientMessageSent& Callback) {
	if (!net.socket.is_open() || !Buffer.Num())
		return false;

	net.socket.async_send_to(asio::buffer(Buffer.GetData(), Buffer.Num()),
								net.endpoint,
								[this, Callback](const asio::error_code& ec, std::size_t bytes_sent) {
									AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
										if (!is_being_destroyed)
											Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
									});
								});
	return true;
}

bool UUDPClient::Connect(const FClientBindOptions& BindOpts) {
	if (net.socket.is_open())
		return false;

	std::string addr = TCHAR_TO_UTF8(*BindOpts.Address);
	std::string port = TCHAR_TO_UTF8(*BindOpts.Port);
	net.resolver.async_resolve(BindOpts.Protocol == EProtocolType::V4 ? udp::v4() : udp::v6(),
										addr, port,
										[this](const asio::error_code &ec, const udp::resolver::results_type &results) {
											resolve(ec, results);
										});
	
	asio::post(thread_pool(), [this]{ run_context_thread(); });
	return true;
}

void UUDPClient::Close() {
	is_closing.Store(true);
	if (net.socket.is_open()) {
		FScopeLock lock(&mutex_error);
		net.socket.shutdown(udp::socket::shutdown_both, error_code);
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
	AsyncTask(ENamedThreads::GameThread, [this]() {
		if (!is_being_destroyed)
			OnClose.Broadcast();
	});
	is_closing.Store(false);
}

void UUDPClient::run_context_thread() {
	FScopeLock lock(&mutex_io);
	error_code.clear();
	net.context.run();
	if (net.socket.is_open() && !is_closing.Load())
		Close();
}

void UUDPClient::resolve(const asio::error_code& error, const udp::resolver::results_type& results) {
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
	net.socket.async_connect(net.endpoint,
								[this](const asio::error_code &ec) {
									conn(ec);
								});
}

void UUDPClient::conn(const asio::error_code& error) {
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
	net.socket.async_receive_from(asio::buffer(recv_buffer.GetData(), recv_buffer.Num()),
									net.endpoint,
									[this](const asio::error_code &ec, const size_t bytes_recvd) {
										receive_from_cb(ec, bytes_recvd);
									});
}

void UUDPClient::consume_recv_buffer() {
	if (recv_buffer.Num() > 0) 
		recv_buffer.Empty();

	if (recv_buffer.Max() != recv_buffer_size) {
		recv_buffer.Shrink();
		recv_buffer.SetNumUninitialized(recv_buffer_size);
	}
}

void UUDPClient::receive_from_cb(const asio::error_code& error, const size_t bytes_recvd) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		AsyncTask(ENamedThreads::GameThread, [this, error]() {
			if (!is_being_destroyed)
				OnError.Broadcast(FErrorCode(error));
		});
		return;
	}

	const TArray<uint8> buffer(recv_buffer.GetData(), bytes_recvd);
	AsyncTask(ENamedThreads::GameThread, [this, buffer, bytes_recvd]() {
		if (!is_being_destroyed)
			OnMessage.Broadcast(buffer, bytes_recvd);
	});
	
	consume_recv_buffer();
	net.socket.async_receive_from(asio::buffer(recv_buffer.GetData(), recv_buffer.Num()),
									net.endpoint,
									[this](const asio::error_code &ec, const size_t bytes_recvd) {
										receive_from_cb(ec, bytes_recvd);
									});
}
