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

#include "UDP/UDPServer.h"

bool UUDPServer::SendStrTo(const FString& Message, const FUDPEndpoint& Endpoint)
{
	if (!UDP.socket.is_open() || Message.IsEmpty())
		return false;

	asio::post(GetThreadPool(), std::bind(&UUDPServer::package_string, this, Message, Endpoint));
	return true;
}

bool UUDPServer::SendBufferTo(const TArray<uint8>& Buffer, const FUDPEndpoint& Endpoint)
{
	if (!UDP.socket.is_open() || Buffer.Num() == 0)
		return false;

	asio::post(GetThreadPool(), std::bind(&UUDPServer::package_buffer, this, Buffer, Endpoint));
	return true;
}

bool UUDPServer::Open()
{
	if (UDP.socket.is_open())
		return false;

	UDP.socket.open(ProtocolType== EProtocolType::V4 ? asio::ip::udp::v4() : asio::ip::udp::v6(),
					ErrorCode);
	if (ErrorCode) {
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	UDP.socket.bind(
		asio::ip::udp::endpoint(ProtocolType == EProtocolType::V4
									? asio::ip::udp::v4()
									: asio::ip::udp::v6(), UdpPort), ErrorCode);
	if (ErrorCode) {
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UUDPServer::run_context_thread, this));
	return true;
}

void UUDPServer::Close()
{
	IsClosing = true;
	UDP.context.stop();
	if (UDP.socket.is_open()) {
		FScopeLock Guard(&MutexError);
		UDP.socket.shutdown(asio::ip::udp::socket::shutdown_both, ErrorCode);
		if (ErrorCode) {
			OnError.Broadcast(ErrorCode);
		}
		UDP.socket.close(ErrorCode);
		if (ErrorCode) {
			OnError.Broadcast(ErrorCode);
		}
	}
	UDP.context.restart();
	OnClose.Broadcast();
	IsClosing = false;
}

void UUDPServer::package_string(const FString& str, const FUDPEndpoint& endpoint)
{
	FScopeLock Guard(&MutexBuffer);
	std::string packaged_str;
	if (!SplitBuffer || str.Len() <= MaxSendBufferSize)
	{
		packaged_str = TCHAR_TO_UTF8(*str);
		UDP.socket.async_send_to(asio::buffer(packaged_str.data(), packaged_str.size()), *endpoint.RawPtr,
								 std::bind(&UUDPServer::send_to, this, asio::placeholders::error,
										   asio::placeholders::bytes_transferred, endpoint));
		return;
	}

	size_t string_offset = 0;
	const size_t max_size = MaxSendBufferSize;
	const size_t str_len = static_cast<size_t>(str.Len());
	while (string_offset < str_len)
	{
		size_t package_size = std::min(max_size, str_len - string_offset);
		FString strshrink = str.Mid(string_offset, package_size);
		packaged_str = TCHAR_TO_UTF8(*strshrink);
		UDP.socket.async_send_to(asio::buffer(packaged_str.data(), packaged_str.size()), *endpoint.RawPtr,
								 std::bind(&UUDPServer::send_to, this, asio::placeholders::error,
										   asio::placeholders::bytes_transferred, endpoint));
		string_offset += package_size;
	}
}

void UUDPServer::package_buffer(const TArray<uint8>& buffer, const FUDPEndpoint& endpoint)
{
	FScopeLock Guard(&MutexBuffer);
	if (!SplitBuffer || buffer.Num() <= MaxSendBufferSize)
	{
		UDP.socket.async_send_to(asio::buffer(buffer.GetData(), buffer.Num()), *endpoint.RawPtr,
								 std::bind(&UUDPServer::send_to, this, asio::placeholders::error,
										   asio::placeholders::bytes_transferred, endpoint));
		return;
	}

	size_t buffer_offset = 0;
	const size_t max_size = MaxSendBufferSize - 1;
	const size_t buf_len = static_cast<size_t>(buffer.Num());
	while (buffer_offset < buf_len)
	{
		size_t package_size = std::min(max_size, buf_len - buffer_offset);
		TArray<uint8> sbuffer;
		sbuffer.Append(buffer.GetData() + buffer_offset, package_size);
		UDP.socket.async_send_to(asio::buffer(sbuffer.GetData(), sbuffer.Num()), *endpoint.RawPtr,
								 std::bind(&UUDPServer::send_to, this, asio::placeholders::error,
										   asio::placeholders::bytes_transferred, endpoint));
		buffer_offset += package_size;
	}
}

void UUDPServer::consume_receive_buffer()
{
	RBuffer.Size = 0;
	if (!RBuffer.RawData.Num() == 0) {
		RBuffer.RawData.Empty();
		RBuffer.RawData.Shrink();
	}
	if (RBuffer.RawData.Num() != MaxReceiveBufferSize)
		RBuffer.RawData.SetNum(MaxReceiveBufferSize);
}

void UUDPServer::run_context_thread()
{
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	if (RBuffer.RawData.Num() != MaxReceiveBufferSize)
		consume_receive_buffer();
	UDP.socket.async_receive_from(asio::buffer(RBuffer.RawData.GetData(), RBuffer.RawData.Num()),
								  UDP.remote_endpoint,
								  std::bind(&UUDPServer::receive_from, this, asio::placeholders::error,
											asio::placeholders::bytes_transferred));
	UDP.context.run();
	if (!IsClosing)
		Close();
}

void UUDPServer::send_to(const asio::error_code& error, const size_t bytes_sent, const FUDPEndpoint& endpoint)
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
			OnError.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
		OnMessageSent.Broadcast(error);
	});
}

void UUDPServer::receive_from(const asio::error_code& error, const size_t bytes_recvd)
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
			OnError.Broadcast(error);
		});
		return;
	}
	RBuffer.Size = bytes_recvd;
	RBuffer.RawData.SetNum(bytes_recvd);
	const FUDPEndpoint ep = UDP.remote_endpoint;
	const FUdpMessage buffer = RBuffer;
	AsyncTask(ENamedThreads::GameThread, [&, buffer, bytes_recvd, ep]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
		OnMessageReceived.Broadcast(buffer, ep);
	});
	consume_receive_buffer();
	UDP.socket.async_receive_from(asio::buffer(RBuffer.RawData.GetData(), RBuffer.RawData.Num()), UDP.remote_endpoint,
								  std::bind(&UUDPServer::receive_from, this, asio::placeholders::error,
											asio::placeholders::bytes_transferred)
	);
}
