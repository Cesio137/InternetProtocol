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

#include "UDP/UDPClient.h"

#include <corecrt_io.h>

bool UUDPClient::SendStr(const FString& message)
{
	if (!UDP.socket.is_open() || message.IsEmpty())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UUDPClient::package_string, this, message));
	return true;
}

bool UUDPClient::SendBuffer(const TArray<uint8>& buffer)
{
	if (!UDP.socket.is_open() || buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UUDPClient::package_buffer, this, buffer));
	return true;
}

bool UUDPClient::Connect()
{
	if (UDP.socket.is_open())
	{
		return false;
	}
	
	asio::post(GetThreadPool(), std::bind(&UUDPClient::run_context_thread, this));
	return true;
}

void UUDPClient::Close()
{
	IsClosing = true;
	UDP.context.stop();
	if (UDP.socket.is_open()) {
		UDP.socket.shutdown(asio::ip::udp::socket::shutdown_both, ErrorCode);
		if (ErrorCode) {
			FScopeLock Guard(&MutexError);
			OnError.Broadcast(ErrorCode);
		}
		UDP.socket.close(ErrorCode);
		if (ErrorCode) {
			FScopeLock Guard(&MutexError);
			OnError.Broadcast(ErrorCode);
		}
	}
	UDP.context.restart();
	OnClose.Broadcast();
	IsClosing = false;
}

void UUDPClient::package_string(const FString& str)
{
	FScopeLock Guard(&MutexBuffer);
	std::string packaged_str;
	if (!SplitBuffer || str.Len() <= MaxSendBufferSize)
	{
		packaged_str = TCHAR_TO_UTF8(*str);
		UDP.socket.async_send_to(asio::buffer(packaged_str.data(), packaged_str.size()), UDP.endpoints,
		                         std::bind(&UUDPClient::send_to, this, asio::placeholders::error,
		                                   asio::placeholders::bytes_transferred));
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
		UDP.socket.async_send_to(asio::buffer(packaged_str.data(), packaged_str.size()), UDP.endpoints,
		                         std::bind(&UUDPClient::send_to, this, asio::placeholders::error,
		                                   asio::placeholders::bytes_transferred));
		string_offset += package_size;
	}
}

void UUDPClient::package_buffer(const TArray<uint8>& buffer)
{
	FScopeLock Guard(&MutexBuffer);
	if (!SplitBuffer || buffer.Num() <= MaxSendBufferSize)
	{
		UDP.socket.async_send_to(asio::buffer(buffer.GetData(), buffer.Num()), UDP.endpoints,
		                         std::bind(&UUDPClient::send_to, this, asio::placeholders::error,
		                                   asio::placeholders::bytes_transferred));
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
		UDP.socket.async_send_to(asio::buffer(sbuffer.GetData(), sbuffer.Num()), UDP.endpoints,
		                         std::bind(&UUDPClient::send_to, this, asio::placeholders::error,
		                                   asio::placeholders::bytes_transferred));
		buffer_offset += package_size;
	}
}

void UUDPClient::consume_receive_buffer()
{
	RBuffer.Size = 0;
	if (!RBuffer.RawData.Num() == 0) {
		RBuffer.RawData.Empty();
		RBuffer.RawData.Shrink();
	}
	if (RBuffer.RawData.Num() != MaxReceiveBufferSize)
		RBuffer.RawData.SetNum(MaxReceiveBufferSize);
}

void UUDPClient::run_context_thread()
{
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	UDP.resolver.async_resolve(ProtocolType == EProtocolType::V4 ? asio::ip::udp::v4() : asio::ip::udp::v6(), TCHAR_TO_UTF8(*Host), TCHAR_TO_UTF8(*Service),
							   std::bind(&UUDPClient::resolve, this, asio::placeholders::error,
										 asio::placeholders::results)
	);
	UDP.context.run();
	if (UDP.socket.is_open() && !IsClosing)
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			Close();
		});
}

void UUDPClient::resolve(const asio::error_code& error, const asio::ip::udp::resolver::results_type& results)
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
	UDP.endpoints = *results.begin();
	UDP.socket.async_connect(UDP.endpoints,
	                         std::bind(&UUDPClient::conn, this, asio::placeholders::error));
}

void UUDPClient::conn(const asio::error_code& error)
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
	consume_receive_buffer();
	UDP.socket.async_receive_from(asio::buffer(RBuffer.RawData.GetData(), RBuffer.RawData.Num()), UDP.endpoints,
	                              std::bind(&UUDPClient::receive_from, this, asio::placeholders::error,
	                                        asio::placeholders::bytes_transferred));
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		OnConnected.Broadcast();
	});
}

void UUDPClient::send_to(const asio::error_code& error, const size_t bytes_sent)
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
		OnBytesTransferred.Broadcast(bytes_sent, 0);
		OnMessageSent.Broadcast();
	});
}

void UUDPClient::receive_from(const asio::error_code& error, const size_t bytes_recvd)
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
	RBuffer.Size = bytes_recvd;
	RBuffer.RawData.SetNum(bytes_recvd);
	const FUdpMessage buffer = RBuffer;
	AsyncTask(ENamedThreads::GameThread, [&, buffer, bytes_recvd]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
		OnMessageReceived.Broadcast(buffer);
	});
	consume_receive_buffer();
	UDP.socket.async_receive_from(asio::buffer(RBuffer.RawData.GetData(), RBuffer.RawData.Num()), UDP.endpoints,
	                              std::bind(&UUDPClient::receive_from, this, asio::placeholders::error,
	                                        asio::placeholders::bytes_transferred)
	);
}
