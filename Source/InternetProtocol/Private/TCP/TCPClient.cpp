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

#include "TCP/TCPClient.h"

bool UTCPClient::SendStr(const FString& message)
{
	if (!TCP.socket.is_open() || message.IsEmpty())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UTCPClient::package_string, this, message));
	return true;
}

bool UTCPClient::SendBuffer(const TArray<uint8>& buffer)
{
	if (!TCP.socket.is_open() || buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UTCPClient::package_buffer, this, buffer));
	return true;
}

bool UTCPClient::Connect()
{
	if (TCP.socket.is_open())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UTCPClient::run_context_thread, this));
	return true;
}

void UTCPClient::Close()
{
	IsClosing = true;
	TCP.context.stop();
	if (TCP.socket.is_open()) {
		FScopeLock Guard(&MutexError);
		TCP.socket.shutdown(asio::ip::udp::socket::shutdown_both, ErrorCode);
		if (ErrorCode) {
			OnError.Broadcast(ErrorCode);
		}
		TCP.socket.close(ErrorCode);
		if (ErrorCode) {
			OnError.Broadcast(ErrorCode);
		}
	}
	TCP.context.restart();
	OnClose.Broadcast();
	IsClosing = false;
}

void UTCPClient::package_string(const FString& str)
{
	FScopeLock Guard(&MutexBuffer);
	std::string packaged_str;
	if (!bSplitBuffer || str.Len() <= MaxSendBufferSize)
	{
		packaged_str = TCHAR_TO_UTF8(*str);
		asio::async_write(TCP.socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred));
		return;
	}

	size_t string_offset = 0;
	const size_t max_size = MaxSendBufferSize; const size_t str_len = static_cast<size_t>(str.Len());
	while (string_offset < str_len)
	{
		size_t package_size = std::min(max_size, str_len - string_offset);
		FString strshrink = str.Mid(string_offset, package_size);
		packaged_str = TCHAR_TO_UTF8(*strshrink);
		asio::async_write(TCP.socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred));
		string_offset += package_size;
	}
}

void UTCPClient::package_buffer(const TArray<uint8>& buffer)
{
	FScopeLock Guard(&MutexBuffer);
	if (!bSplitBuffer || buffer.Num() <= MaxSendBufferSize)
	{
		asio::async_write(TCP.socket, asio::buffer(buffer.GetData(), buffer.Num()),
		                  std::bind(&UTCPClient::write, this, asio::placeholders::error,
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
		if (sbuffer.Last() != '\0')
		{
			sbuffer.Add('\0');
		}
		asio::async_write(TCP.socket, asio::buffer(sbuffer.GetData(), sbuffer.Num()),
		                  std::bind(&UTCPClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred));
		buffer_offset += package_size;
	}
}

void UTCPClient::consume_response_buffer()
{
	const size_t size = ResponseBuffer.size();
	if (size > 0)
		ResponseBuffer.consume(ResponseBuffer.size());
}

void UTCPClient::run_context_thread()
{
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	TCP.resolver.async_resolve(TCHAR_TO_UTF8(*Host), TCHAR_TO_UTF8(*Service),
							   std::bind(&UTCPClient::resolve, this, asio::placeholders::error,
										 asio::placeholders::results));
	TCP.context.run();
	if (!IsClosing)
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			Close();
		});
}

void UTCPClient::resolve(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
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
	TCP.endpoints = endpoints;
	asio::async_connect(TCP.socket, TCP.endpoints,
	                    std::bind(&UTCPClient::conn, this, asio::placeholders::error)
	);
}

void UTCPClient::conn(const asio::error_code& error)
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
	consume_response_buffer();
	asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UTCPClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		OnConnected.Broadcast();
	});
}

void UTCPClient::write(const asio::error_code& error, const size_t bytes_sent)
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
			OnMessageSent.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent, error]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
		OnMessageSent.Broadcast(error);
	});
}

void UTCPClient::read(const asio::error_code& error, const size_t bytes_recvd)
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
	FTcpMessage rbuffer;
	rbuffer.Size = bytes_recvd;
	rbuffer.RawData.SetNum(bytes_recvd);
	asio::buffer_copy(asio::buffer(rbuffer.RawData.GetData(), bytes_recvd), ResponseBuffer.data());
	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd, rbuffer]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
		OnMessageReceived.Broadcast(rbuffer);
	});
	consume_response_buffer();
	asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UTCPClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
}

bool UTCPClientSsl::SendStr(const FString& message)
{
	if (!TCP.ssl_socket.lowest_layer().is_open() || message.IsEmpty())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UTCPClientSsl::package_string, this, message));
	return true;
}

bool UTCPClientSsl::SendBuffer(const TArray<uint8>& buffer)
{
	if (!TCP.ssl_socket.lowest_layer().is_open() || buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UTCPClientSsl::package_buffer, this, buffer));
	return true;
}

bool UTCPClientSsl::Connect()
{
	if (TCP.ssl_socket.next_layer().is_open())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UTCPClientSsl::run_context_thread, this));
	return true;
}

void UTCPClientSsl::Close()
{
	IsClosing = true;
	if (TCP.ssl_socket.next_layer().is_open()) {
		FScopeLock Guard(&MutexError);
		TCP.ssl_socket.shutdown(ErrorCode);
		if (ErrorCode) {
			OnError.Broadcast(ErrorCode);
		}
		TCP.ssl_socket.next_layer().close(ErrorCode);
		if (ErrorCode) {
			OnError.Broadcast(ErrorCode);
		}
	}
	TCP.context.stop();
	TCP.context.restart();
	TCP.ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(TCP.context, TCP.ssl_context);
	OnClose.Broadcast();
	IsClosing = false;
}

void UTCPClientSsl::package_string(const FString& str)
{
	FScopeLock Guard(&MutexBuffer);
	std::string packaged_str;
	if (!SplitBuffer || str.Len() <= MaxSendBufferSize)
	{
		packaged_str = TCHAR_TO_UTF8(*str);
		asio::async_write(TCP.ssl_socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPClientSsl::write, this, asio::placeholders::error,
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
		asio::async_write(TCP.ssl_socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred));
		string_offset += package_size;
	}
}

void UTCPClientSsl::package_buffer(const TArray<uint8>& buffer)
{
	FScopeLock Guard(&MutexBuffer);
	if (!SplitBuffer || buffer.Num() <= MaxSendBufferSize)
	{
		asio::async_write(TCP.ssl_socket, asio::buffer(buffer.GetData(), buffer.Num()),
		                  std::bind(&UTCPClientSsl::write, this, asio::placeholders::error,
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
		if (sbuffer.Last() != '\0')
		{
			sbuffer.Add('\0');
		}
		asio::async_write(TCP.ssl_socket, asio::buffer(sbuffer.GetData(), sbuffer.Num()),
		                  std::bind(&UTCPClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred));
		buffer_offset += package_size;
	}
}

void UTCPClientSsl::consume_response_buffer()
{
	const size_t size = ResponseBuffer.size();
	if (size > 0)
		ResponseBuffer.consume(ResponseBuffer.size());
}

void UTCPClientSsl::run_context_thread()
{
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	TCP.resolver.async_resolve(TCHAR_TO_UTF8(*Host), TCHAR_TO_UTF8(*Service),
							   std::bind(&UTCPClientSsl::resolve, this, asio::placeholders::error,
										 asio::placeholders::results)
	);
	TCP.context.run();
	if (!IsClosing)
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			Close();
		});
}

void UTCPClientSsl::resolve(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
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
	TCP.endpoints = endpoints;
	asio::async_connect(TCP.ssl_socket.lowest_layer(), TCP.endpoints,
	                    std::bind(&UTCPClientSsl::conn, this, asio::placeholders::error)
	);
}

void UTCPClientSsl::conn(const asio::error_code& error)
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
	TCP.ssl_socket.async_handshake(asio::ssl::stream_base::client,
	                               std::bind(&UTCPClientSsl::ssl_handshake, this,
	                                         asio::placeholders::error));
}

void UTCPClientSsl::ssl_handshake(const asio::error_code& error)
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
	consume_response_buffer();
	asio::async_read(
		TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
		std::bind(&UTCPClientSsl::read, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred));
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		OnConnected.Broadcast();
	});
}

void UTCPClientSsl::write(const asio::error_code& error, const size_t bytes_sent)
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
			OnMessageSent.Broadcast(error);
			OnError.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent, error]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
		OnMessageSent.Broadcast(error);
	});
}

void UTCPClientSsl::read(const asio::error_code& error, const size_t bytes_recvd)
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
	FTcpMessage rbuffer;
	rbuffer.Size = bytes_recvd;
	rbuffer.RawData.SetNum(bytes_recvd);
	asio::buffer_copy(asio::buffer(rbuffer.RawData.GetData(), bytes_recvd), ResponseBuffer.data());
	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd, rbuffer]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
		OnMessageReceived.Broadcast(rbuffer);
	});
	consume_response_buffer();
	asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UTCPClientSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
}
