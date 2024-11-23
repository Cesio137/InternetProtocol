/*
 * Copyright (c) 2023-2024 Nathan Miguel
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

bool UUDPClient::Send(const FString& message)
{
	if (!Thread_Pool.IsValid() || !IsConnected() || message.IsEmpty())
	{
		return false;
	}

	asio::post(*Thread_Pool, std::bind(&UUDPClient::package_string, this, message));
	return true;
}

bool UUDPClient::SendRaw(const TArray<uint8>& buffer)
{
	if (!IsConnected() || buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(*Thread_Pool, std::bind(&UUDPClient::package_buffer, this, buffer));
	return true;
}

bool UUDPClient::AsyncRead()
{
	if (!IsConnected())
	{
		return false;
	}

	UDP.socket.async_receive_from(asio::buffer(RBuffer.RawData.GetData(), RBuffer.RawData.Num()), UDP.endpoints,
	                              std::bind(&UUDPClient::receive_from, this, asio::placeholders::error,
	                                        asio::placeholders::bytes_transferred)
	);
	return true;
}

bool UUDPClient::Connect()
{
	if (!Thread_Pool.IsValid() || IsConnected())
	{
		return false;
	}
	
	asio::post(*Thread_Pool, std::bind(&UUDPClient::run_context_thread, this));
	return true;
}

void UUDPClient::Close()
{
	asio::error_code ec_shutdown;
	asio::error_code ec_close;
	UDP.context.stop();
	UDP.socket.shutdown(asio::ip::udp::socket::shutdown_both, ec_shutdown);
	UDP.socket.close(ec_close);
	if (ShouldStopContext)
	{
		return;
	}
	if (ec_shutdown)
	{
		ensureMsgf(!ec_shutdown, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ec_shutdown.value(),
		           ec_shutdown.message().c_str());
		OnError.Broadcast(ec_shutdown.value(), ec_shutdown.message().c_str());
	}
	if (ec_close)
	{
		ensureMsgf(!ec_close, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ec_close.value(),
		           ec_close.message().c_str());
		OnError.Broadcast(ec_close.value(), ec_close.message().c_str());
	}
	OnClose.Broadcast();
}

void UUDPClient::package_string(const FString& str)
{
	MutexBuffer.Lock();
	std::string packaged_str;
	if (!SplitBuffer || str.Len() <= MaxSendBufferSize)
	{
		packaged_str = TCHAR_TO_UTF8(*str);
		UDP.socket.async_send_to(asio::buffer(packaged_str.data(), packaged_str.size()), UDP.endpoints,
		                         std::bind(&UUDPClient::send_to, this, asio::placeholders::error,
		                                   asio::placeholders::bytes_transferred));
		MutexBuffer.Unlock();
		return;
	}

	size_t string_offset = 0;
	const size_t max_size = MaxSendBufferSize;
	while (string_offset < str.Len())
	{
		size_t package_size = std::min(max_size, str.Len() - string_offset);
		FString strshrink = str.Mid(string_offset, package_size);
		packaged_str = TCHAR_TO_UTF8(*strshrink);
		UDP.socket.async_send_to(asio::buffer(packaged_str.data(), packaged_str.size()), UDP.endpoints,
		                         std::bind(&UUDPClient::send_to, this, asio::placeholders::error,
		                                   asio::placeholders::bytes_transferred));
		string_offset += package_size;
	}
	MutexBuffer.Unlock();
}

void UUDPClient::package_buffer(const TArray<uint8>& buffer)
{
	MutexBuffer.Lock();
	if (!SplitBuffer || buffer.Num() <= MaxSendBufferSize)
	{
		UDP.socket.async_send_to(asio::buffer(buffer.GetData(), buffer.Num() * sizeof(char)), UDP.endpoints,
		                         std::bind(&UUDPClient::send_to, this, asio::placeholders::error,
		                                   asio::placeholders::bytes_transferred)
		);
		MutexBuffer.Unlock();
		return;
	}

	size_t buffer_offset = 0;
	const size_t max_size = MaxSendBufferSize - 1;
	while (buffer_offset < buffer.Num() * sizeof(char))
	{
		size_t package_size = std::min(max_size, buffer.Num() - buffer_offset);
		TArray<uint8> sbuffer;
		sbuffer.Append(buffer.GetData() + buffer_offset, package_size);
		UDP.socket.async_send_to(asio::buffer(sbuffer.GetData(), sbuffer.Num() * sizeof(char)), UDP.endpoints,
		                         std::bind(&UUDPClient::send_to, this, asio::placeholders::error,
		                                   asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
	}
	MutexBuffer.Unlock();
}

void UUDPClient::run_context_thread()
{
	MutexIO.Lock();
	while (UDP.attemps_fail <= MaxAttemp && !ShouldStopContext)
	{
		if (UDP.attemps_fail > 0)
		{
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				OnConnectionWillRetry.Broadcast(UDP.attemps_fail);
			});
		}
		UDP.error_code.clear();
		UDP.resolver.async_resolve(asio::ip::udp::v4(), TCHAR_TO_UTF8(*Host), TCHAR_TO_UTF8(*Service),
		                           std::bind(&UUDPClient::resolve, this, asio::placeholders::error,
		                                     asio::placeholders::results)
		);
		UDP.context.run();
		UDP.context.restart();
		if (!UDP.error_code)
		{
			break;
		}
		UDP.attemps_fail++;
		FPlatformProcess::Sleep(Timeout);
	}
	consume_receive_buffer();
	UDP.attemps_fail = 0;
	MutexIO.Unlock();
}

void UUDPClient::resolve(const asio::error_code& error, const asio::ip::udp::resolver::results_type& results)
{
	if (error)
	{
		UDP.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	UDP.endpoints = *results.begin();
	UDP.socket.async_connect(UDP.endpoints,
	                         std::bind(&UUDPClient::conn, this, asio::placeholders::error)
	);
}

void UUDPClient::conn(const asio::error_code& error)
{
	if (error)
	{
		UDP.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	consume_receive_buffer();
	UDP.socket.async_receive_from(asio::buffer(RBuffer.RawData.GetData(), RBuffer.RawData.Num()), UDP.endpoints,
	                              std::bind(&UUDPClient::receive_from, this, asio::placeholders::error,
	                                        asio::placeholders::bytes_transferred)
	);
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnConnected.Broadcast();
	});
}

void UUDPClient::send_to(const asio::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnMessageSent.Broadcast(bytes_sent);
	});
}

void UUDPClient::receive_from(const asio::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
					   error.message().c_str());
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	UDP.attemps_fail = 0;
	RBuffer.Size = bytes_recvd;
	RBuffer.RawData.SetNum(bytes_recvd);
	const FUdpMessage buffer = RBuffer;
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnMessageReceived.Broadcast(buffer);
	});
	consume_receive_buffer();
	UDP.socket.async_receive_from(asio::buffer(RBuffer.RawData.GetData(), RBuffer.RawData.Num()), UDP.endpoints,
	                              std::bind(&UUDPClient::receive_from, this, asio::placeholders::error,
	                                        asio::placeholders::bytes_transferred)
	);
}
