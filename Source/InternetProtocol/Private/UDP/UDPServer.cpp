/** 
 * Copyright © 2025 Nathan Miguel
 */

#include "udp/udpserver.h"

void UUDPServer::BeginDestroy() {
	is_being_destroyed = true;
	if (net.socket.is_open())
		Close();
	UObject::BeginDestroy();
}

void UUDPServer::AddToRoot() {
	Super::AddToRoot();
}

void UUDPServer::RemoveFromRoot() {
	Super::RemoveFromRoot();
}

bool UUDPServer::IsRooted() {
	return Super::IsRooted();
}

void UUDPServer::MarkPendingKill() {
	Super::MarkPendingKill();
}

bool UUDPServer::IsOpen() {
	return net.socket.is_open();
}

FUdpEndpoint UUDPServer::LocalEndpoint() {
	udp::endpoint ep = net.socket.local_endpoint();
	return FUdpEndpoint(ep);
}

FUdpEndpoint UUDPServer::RemoteEndpoint() {
	udp::endpoint ep = net.socket.remote_endpoint();
	return FUdpEndpoint(ep);
}

void UUDPServer::SetRecvBufferSize(int Val) {
	recv_buffer_size = Val < 0 ? 0 : Val;
}

int UUDPServer::GetRecvBufferSize() {
	return recv_buffer_size;
}

FErrorCode UUDPServer::GetErrorCode() {
	return FErrorCode(error_code);
}

bool UUDPServer::SendTo(const FString& Message, const FUdpEndpoint& Endpoint,
	const FDelegateUdpServerMessageSent& Callback) {
	if (!net.socket.is_open() || Message.IsEmpty())
		return false;

	net.socket.async_send_to(asio::buffer(TCHAR_TO_UTF8(*Message),  Message.Len()),
								Endpoint.Endpoint,
								[this, Callback](const asio::error_code &ec, size_t bytes_sent) {
									AsyncTask(ENamedThreads::GameThread, [this, ec, Callback, bytes_sent]() {
										if (!is_being_destroyed)
											Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
									});
								});
	return true;
}

bool UUDPServer::SendBufferTo(const TArray<uint8>& Buffer, const FUdpEndpoint& Endpoint,
	const FDelegateUdpServerMessageSent& Callback) {
	if (!net.socket.is_open() || !Buffer.Num())
		return false;

	net.socket.async_send_to(asio::buffer(Buffer.GetData(),  Buffer.Num() * sizeof(Buffer.GetTypeSize())),
								Endpoint.Endpoint,
								[this, Callback](const asio::error_code &ec, size_t bytes_sent) {
									AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
										if (!is_being_destroyed)
											Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
									});
								});
	return true;
}

bool UUDPServer::Bind(const FServerBindOptions& BindOpts) {
	if (net.socket.is_open())
		return false;

	net.socket.open(BindOpts.Protocol == EProtocolType::V4 ? udp::v4() : udp::v6(),
					error_code);
	if (error_code) {
		FScopeLock lock(&mutex_error);
		if (!is_being_destroyed)
			OnError.Broadcast(FErrorCode(error_code));
		return false;
	}
	net.socket.set_option(asio::socket_base::reuse_address(BindOpts.bReuse_Address));
	const udp::endpoint endpoint = BindOpts.Address.IsEmpty() ?
								udp::endpoint(BindOpts.Protocol == EProtocolType::V4
												? udp::v4()
												: udp::v6(),
												BindOpts.Port) :
								udp::endpoint(
									make_address(TCHAR_TO_UTF8(*BindOpts.Address)),
									BindOpts.Port
								);
	net.socket.bind(endpoint, error_code);
	if (error_code) {
		FScopeLock lock(&mutex_error);
		if (!is_being_destroyed)
			OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	asio::post(thread_pool(), [this]{ run_context_thread(); });
	return true;
}

void UUDPServer::Close() {
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

void UUDPServer::run_context_thread() {
	FScopeLock lock(&mutex_io);
	error_code.clear();
	consume_receive_buffer();
	net.socket.async_receive_from(asio::buffer(recv_buffer.GetData(), recv_buffer.Num() * sizeof(recv_buffer.GetTypeSize())),
								  net.remote_endpoint,
								  [this](const asio::error_code &ec, const size_t bytes_recvd) {
									  receive_from_cb(ec, bytes_recvd);
								  });
	net.context.run();
	if (net.socket.is_open() && !is_closing.Load())
		Close();
}

void UUDPServer::consume_receive_buffer() {
	if (recv_buffer.Num() > 0) 
		recv_buffer.Empty();

	if (recv_buffer.GetAllocatedSize() != recv_buffer_size) {
		recv_buffer.Shrink();
		recv_buffer.SetNumUninitialized(recv_buffer_size);
	}
}

void UUDPServer::receive_from_cb(const asio::error_code& error, const size_t bytes_recvd) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		AsyncTask(ENamedThreads::GameThread, [this]() {
			if (!is_being_destroyed)
				OnError.Broadcast(FErrorCode(error_code));
		});
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [this, bytes_recvd]() {
		if (!is_being_destroyed)
			OnMessage.Broadcast(recv_buffer, bytes_recvd, FUdpEndpoint(net.remote_endpoint));
	});

	consume_receive_buffer();
	net.socket.async_receive_from(asio::buffer(recv_buffer.GetData(), recv_buffer.Num() * sizeof(recv_buffer.GetTypeSize())),
									net.remote_endpoint,
									[this](const asio::error_code &ec, const size_t bytes_recvd) {
										receive_from_cb(ec, bytes_recvd);
									});
}


