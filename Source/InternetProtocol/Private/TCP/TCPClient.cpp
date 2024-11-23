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

#include "TCP/TCPClient.h"

bool UTCPClient::Send(const FString& message)
{
	if (!IsConnected() || message.IsEmpty())
	{
		return false;
	}

	asio::post(*ThreadPool, std::bind(&UTCPClient::package_string, this, message));
	return true;
}

bool UTCPClient::SendRaw(const TArray<uint8>& buffer)
{
	if (!IsConnected() || buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(*ThreadPool, std::bind(&UTCPClient::package_buffer, this, buffer));
	return true;
}

bool UTCPClient::AsyncRead()
{
	if (!IsConnected())
	{
		return false;
	}

	asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(2),
	                 std::bind(&UTCPClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred));
	return true;
}

bool UTCPClient::Connect()
{
	if (!ThreadPool.IsValid() || IsConnected())
	{
		return false;
	}

	asio::post(*ThreadPool, std::bind(&UTCPClient::run_context_thread, this));
	return true;
}

void UTCPClient::Close()
{
	asio::error_code ec_shutdown;
	asio::error_code ec_close;
	TCP.context.stop();
	TCP.socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec_shutdown);
	TCP.socket.close(ec_close);
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

void UTCPClient::package_string(const FString& str)
{
	MutexBuffer.Lock();
	std::string packaged_str;
	if (!bSplitBuffer || str.Len() <= MaxSendBufferSize)
	{
		packaged_str = TCHAR_TO_UTF8(*str);
		asio::async_write(TCP.socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
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
		asio::async_write(TCP.socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		string_offset += package_size;
	}
	MutexBuffer.Unlock();
}

void UTCPClient::package_buffer(const TArray<uint8>& buffer)
{
	MutexBuffer.Lock();
	if (!bSplitBuffer || buffer.Num() * sizeof(char) <= MaxSendBufferSize)
	{
		asio::async_write(TCP.socket, asio::buffer(buffer.GetData(), buffer.Num() * sizeof(char)),
		                  std::bind(&UTCPClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		MutexBuffer.Unlock();
		return;
	}

	size_t buffer_offset = 0;
	const size_t max_size = MaxSendBufferSize - 1;
	while (buffer_offset < buffer.Num() * sizeof(char))
	{
		size_t package_size = std::min(max_size, (buffer.Num() * sizeof(char)) - buffer_offset);
		TArray<uint8> sbuffer;
		sbuffer.Append(buffer.GetData() + buffer_offset, package_size);
		if (sbuffer.Last() != '\0')
		{
			sbuffer.Add('\0');
		}
		asio::async_write(TCP.socket, asio::buffer(sbuffer.GetData(), sbuffer.Num() * sizeof(char)),
		                  std::bind(&UTCPClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
	}
	MutexBuffer.Unlock();
}

void UTCPClient::run_context_thread()
{
	MutexIO.Lock();
	while (TCP.attemps_fail <= MaxAttemp && !ShouldStopContext)
	{
		if (TCP.attemps_fail > 0)
		{
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				OnConnectionWillRetry.Broadcast(TCP.attemps_fail);
			});
		}
		TCP.error_code.clear();
		TCP.resolver.async_resolve(TCHAR_TO_UTF8(*Host), TCHAR_TO_UTF8(*Service),
		                           std::bind(&UTCPClient::resolve, this, asio::placeholders::error,
		                                     asio::placeholders::results)
		);
		TCP.context.run();
		TCP.context.restart();
		if (!TCP.error_code)
		{
			break;
		}
		TCP.attemps_fail++;
		FPlatformProcess::Sleep(Timeout);
	}
	consume_response_buffer();
	TCP.attemps_fail = 0;
	MutexIO.Unlock();
}

void UTCPClient::resolve(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		TCP.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	TCP.endpoints = endpoints;
	asio::async_connect(TCP.socket, TCP.endpoints,
	                    std::bind(&UTCPClient::conn, this, asio::placeholders::error)
	);
}

void UTCPClient::conn(const asio::error_code& error)
{
	if (error)
	{
		TCP.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	consume_response_buffer();
	asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UTCPClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnConnected.Broadcast();
	});
}

void UTCPClient::write(const asio::error_code& error, const size_t bytes_sent)
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

void UTCPClient::read(const asio::error_code& error, const size_t bytes_recvd)
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
	FTcpMessage rbuffer;
	rbuffer.Size = bytes_recvd;
	rbuffer.RawData.SetNum(bytes_recvd);
	asio::buffer_copy(asio::buffer(rbuffer.RawData.GetData(), bytes_recvd), ResponseBuffer.data());
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnMessageReceived.Broadcast(rbuffer);
	});
	consume_response_buffer();
	asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UTCPClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
}

bool UTCPClientSsl::Send(const FString& message)
{
	if (!IsConnected() || message.IsEmpty())
	{
		return false;
	}

	asio::post(*ThreadPool, std::bind(&UTCPClientSsl::package_string, this, message));
	return true;
}

bool UTCPClientSsl::SendRaw(const TArray<uint8>& buffer)
{
	if (!IsConnected() || buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(*ThreadPool, std::bind(&UTCPClientSsl::package_buffer, this, buffer));
	return true;
}

bool UTCPClientSsl::AsyncRead()
{
	if (!IsConnected())
	{
		return false;
	}

	asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(2),
	                 std::bind(&UTCPClientSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred));
	return true;
}

bool UTCPClientSsl::Connect()
{
	if (IsConnected())
	{
		return false;
	}

	asio::post(*ThreadPool, std::bind(&UTCPClientSsl::run_context_thread, this));
	return true;
}

void UTCPClientSsl::Close()
{
	asio::error_code ec_shutdown;
	asio::error_code ec_close;
	TCP.context.stop();
	TCP.ssl_socket.shutdown(ec_shutdown);
	TCP.ssl_socket.lowest_layer().close(ec_close);
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

void UTCPClientSsl::package_string(const FString& str)
{
	MutexBuffer.Lock();
	std::string packaged_str;
	if (!SplitBuffer || str.Len() <= MaxSendBufferSize)
	{
		packaged_str = TCHAR_TO_UTF8(*str);
		asio::async_write(TCP.ssl_socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
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
		asio::async_write(TCP.ssl_socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		string_offset += package_size;
	}
	MutexBuffer.Unlock();
}

void UTCPClientSsl::package_buffer(const TArray<uint8>& buffer)
{
	MutexBuffer.Lock();
	if (!SplitBuffer || buffer.Num() * sizeof(char) <= MaxSendBufferSize)
	{
		asio::async_write(TCP.ssl_socket, asio::buffer(buffer.GetData(), buffer.Num() * sizeof(char)),
		                  std::bind(&UTCPClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		MutexBuffer.Unlock();
		return;
	}

	size_t buffer_offset = 0;
	const size_t max_size = MaxSendBufferSize - 1;
	while (buffer_offset < buffer.Num() * sizeof(char))
	{
		size_t package_size = std::min(max_size, (buffer.Num() * sizeof(char)) - buffer_offset);
		TArray<uint8> sbuffer;
		sbuffer.Append(buffer.GetData() + buffer_offset, package_size);
		if (sbuffer.Last() != '\0')
		{
			sbuffer.Add('\0');
		}
		asio::async_write(TCP.ssl_socket, asio::buffer(sbuffer.GetData(), sbuffer.Num() * sizeof(char)),
		                  std::bind(&UTCPClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
	}
	MutexBuffer.Unlock();
}

void UTCPClientSsl::run_context_thread()
{
	MutexIO.Lock();
	while (TCP.attemps_fail <= MaxAttemp && !ShouldStopContext)
	{
		if (TCP.attemps_fail > 0)
		{
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				OnConnectionWillRetry.Broadcast(TCP.attemps_fail);
			});
		}
		TCP.error_code.clear();
		TCP.resolver.async_resolve(TCHAR_TO_UTF8(*Host), TCHAR_TO_UTF8(*Service),
		                           std::bind(&UTCPClientSsl::resolve, this, asio::placeholders::error,
		                                     asio::placeholders::results)
		);
		TCP.context.run();
		TCP.context.restart();
		if (!TCP.error_code)
		{
			break;
		}
		TCP.attemps_fail++;
		FPlatformProcess::Sleep(Timeout);
	}
	consume_response_buffer();
	TCP.attemps_fail = 0;
	MutexIO.Unlock();
}

void UTCPClientSsl::resolve(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		TCP.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	TCP.endpoints = endpoints;
	asio::async_connect(TCP.ssl_socket.lowest_layer(), TCP.endpoints,
	                    std::bind(&UTCPClientSsl::conn, this, asio::placeholders::error)
	);
}

void UTCPClientSsl::conn(const asio::error_code& error)
{
	if (error)
	{
		TCP.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	TCP.ssl_socket.async_handshake(asio::ssl::stream_base::client,
	                               std::bind(&UTCPClientSsl::ssl_handshake, this,
	                                         asio::placeholders::error));
}

void UTCPClientSsl::ssl_handshake(const asio::error_code& error)
{
	if (error)
	{
		TCP.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	consume_response_buffer();
	asio::async_read(
		TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
		std::bind(&UTCPClientSsl::read, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred));
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnConnected.Broadcast();
	});
}

void UTCPClientSsl::write(const asio::error_code& error, const size_t bytes_sent)
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

void UTCPClientSsl::read(const asio::error_code& error, const size_t bytes_recvd)
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
	FTcpMessage rbuffer;
	rbuffer.Size = bytes_recvd;
	rbuffer.RawData.SetNum(bytes_recvd);
	asio::buffer_copy(asio::buffer(rbuffer.RawData.GetData(), bytes_recvd), ResponseBuffer.data());
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnMessageReceived.Broadcast(rbuffer);
	});
	consume_response_buffer();
	asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UTCPClientSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
}
