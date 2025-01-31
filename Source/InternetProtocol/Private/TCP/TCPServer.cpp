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

#include "TCP/TCPServer.h"

bool UTCPServer::SendStrTo(const FString& Message, const FTCPSocket& Socket)
{
	if (!Socket.SmartPtr->is_open() || Message.IsEmpty())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UTCPServer::package_string, this, Message, Socket.SmartPtr));
	return true;
}

bool UTCPServer::SendBufferTo(const TArray<uint8>& Buffer, const FTCPSocket& Socket)
{
	if (!Socket.SmartPtr->is_open() || Buffer.Num() == 0)
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UTCPServer::package_buffer, this, Buffer, Socket.SmartPtr));
	return true;
}

bool UTCPServer::Open()
{
	if (TCP.acceptor.is_open())
	{
		return false;
	}

	asio::ip::tcp::endpoint endpoint(TcpProtocol == EProtocolType::V4
		                                 ? asio::ip::tcp::v4()
		                                 : asio::ip::tcp::v6(), TcpPort);
	ErrorCode = asio::error_code();
	TCP.acceptor.open(TcpProtocol == EProtocolType::V4
		                  ? asio::ip::tcp::v4()
		                  : asio::ip::tcp::v6(), ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	TCP.acceptor.set_option(asio::socket_base::reuse_address(true), ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	TCP.acceptor.bind(endpoint, ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	TCP.acceptor.listen(Backlog, ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UTCPServer::run_context_thread, this));
	return true;
}

void UTCPServer::Close()
{
	IsClosing = true;
	if (TCP.sockets.Num() > 0)
	{
		for (const socket_ptr& socket : TCP.sockets)
		{
			if (socket->is_open())
			{
				FScopeLock Guard(&MutexError);
				socket->shutdown(asio::ip::tcp::socket::shutdown_both, ErrorCode);
				bool has_error = false;
				if (ErrorCode)
				{
					has_error = true;
					OnSocketDisconnected.Broadcast(ErrorCode, socket);
				}
				socket->close(ErrorCode);
				if (ErrorCode)
				{
					has_error = true;
					OnSocketDisconnected.Broadcast(ErrorCode, socket);
				}
				if (!has_error)
				{
					OnSocketDisconnected.Broadcast(FErrorCode(), socket);
				}
			}
		}
	}
	TCP.context.stop();
	if (TCP.sockets.Num() > 0)
	{
		TCP.sockets.Empty();
	}
	if (ListenerBuffer.Num() > 0)
	{
		ListenerBuffer.Empty();
	}
	if (TCP.acceptor.is_open())
	{
		FScopeLock Guard(&MutexError);
		TCP.acceptor.close(ErrorCode);
		OnError.Broadcast(ErrorCode);
	}
	TCP.context.restart();
	TCP.acceptor = asio::ip::tcp::acceptor(TCP.context);
	OnClose.Broadcast();
	IsClosing = false;
}

void UTCPServer::DisconnectSocket(const FTCPSocket& Socket)
{
	bool has_error = false;
	if (Socket.SmartPtr->is_open())
	{
		FScopeLock Guard(&MutexError);
		Socket.SmartPtr->shutdown(asio::ip::tcp::socket::shutdown_both, ErrorCode);
		if (ErrorCode)
		{
			has_error = true;
			OnSocketDisconnected.Broadcast(ErrorCode, Socket.SmartPtr);
		}
		Socket.SmartPtr->close(ErrorCode);
		if (ErrorCode)
		{
			has_error = true;
			OnSocketDisconnected.Broadcast(ErrorCode, Socket.SmartPtr);
		}
	}
	if (ListenerBuffer.Contains(Socket.SmartPtr))
	{
		ListenerBuffer.Remove(Socket.SmartPtr);
	}
	if (TCP.sockets.Contains(Socket.SmartPtr))
	{
		TCP.sockets.Remove(Socket.SmartPtr);
	}
	if (!has_error)
	{
		OnSocketDisconnected.Broadcast(FErrorCode(), Socket.SmartPtr);
	}
}

void UTCPServer::disconnect_socket_after_error(const asio::error_code& error, const socket_ptr& socket)
{
	if (socket->is_open())
	{
		FScopeLock Guard(&MutexError);
		socket->shutdown(asio::ip::tcp::socket::shutdown_both, ErrorCode);
		if (ErrorCode)
		{
			OnSocketDisconnected.Broadcast(ErrorCode, socket);
		}
		socket->close(ErrorCode);
		if (ErrorCode)
		{
			OnSocketDisconnected.Broadcast(ErrorCode, socket);
		}
	}
	if (ListenerBuffer.Contains(socket))
	{
		ListenerBuffer.Remove(socket);
	}
	if (TCP.sockets.Contains(socket))
	{
		TCP.sockets.Remove(socket);
	}
	OnSocketDisconnected.Broadcast(error, socket);
}

void UTCPServer::package_string(const FString& str, const socket_ptr& socket)
{
	FScopeLock Guard(&MutexBuffer);
	std::string packaged_str;
	if (!bSplitBuffer || str.Len() <= MaxSendBufferSize)
	{
		packaged_str = TCHAR_TO_UTF8(*str);
		asio::async_write(*socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPServer::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, socket));
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
		asio::async_write(*socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPServer::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, socket));
		string_offset += package_size;
	}
}

void UTCPServer::package_buffer(const TArray<uint8>& buffer, const socket_ptr& socket)
{
	FScopeLock Guard(&MutexBuffer);
	if (!bSplitBuffer || buffer.Num() <= MaxSendBufferSize)
	{
		asio::async_write(*socket, asio::buffer(buffer.GetData(), buffer.Num()),
		                  std::bind(&UTCPServer::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, socket));
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
		asio::async_write(*socket, asio::buffer(sbuffer.GetData(), sbuffer.Num()),
		                  std::bind(&UTCPServer::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, socket));
		buffer_offset += package_size;
	}
}

void UTCPServer::consume_response_buffer(const socket_ptr& socket)
{
	if (!ListenerBuffer.Contains(socket))
	{
		return;
	}
	asio::streambuf& streambuf = *ListenerBuffer[socket];
	const size_t size = streambuf.size();
	if (size > 0)
	{
		streambuf.consume(size);
	}
}

void UTCPServer::run_context_thread()
{
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	socket_ptr conn_socket = MakeShared<asio::ip::tcp::socket>(TCP.context);
	TCP.acceptor.async_accept(
		*conn_socket, std::bind(&UTCPServer::accept, this, asio::placeholders::error, conn_socket));
	TCP.context.run();
	if (!IsClosing)
	{
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			Close();
		});
	}
}

void UTCPServer::accept(const asio::error_code& error, const socket_ptr& socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			if (!IsClosing)
			{
				disconnect_socket_after_error(error, socket);
			}
		});
		if (TCP.acceptor.is_open())
		{
			socket_ptr conn_socket = MakeShared<asio::ip::tcp::socket>(TCP.context);
			TCP.acceptor.async_accept(
				*conn_socket, std::bind(&UTCPServer::accept, this, asio::placeholders::error, conn_socket));
		}
		return;
	}

	if (TCP.sockets.Num() < Backlog)
	{
		TCP.sockets.Add(socket);
		TSharedPtr<asio::streambuf> response_buffer = MakeShared<asio::streambuf>();
		ListenerBuffer.Add(socket, response_buffer);
		asio::async_read(*socket, *ListenerBuffer[socket], asio::transfer_at_least(1),
		                 std::bind(&UTCPServer::read, this, asio::placeholders::error,
		                           asio::placeholders::bytes_transferred, socket));
		AsyncTask(ENamedThreads::GameThread, [&, socket]()
		{
			OnSocketAccepted.Broadcast(socket);
		});
	}
	else
	{
		FScopeLock Guard(&MutexError);
		AsyncTask(ENamedThreads::GameThread, [&, error, socket]()
		{
			if (!IsClosing)
			{
				disconnect_socket_after_error(error, socket);
			}
		});
	}

	if (TCP.acceptor.is_open())
	{
		socket_ptr conn_socket = MakeShared<asio::ip::tcp::socket>(TCP.context);
		TCP.acceptor.async_accept(
			*conn_socket, std::bind(&UTCPServer::accept, this, asio::placeholders::error, conn_socket));
	}
}

void UTCPServer::write(const asio::error_code& error, const size_t bytes_sent, const socket_ptr& socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnMessageSent.Broadcast(error, socket);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent, socket]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
		OnMessageSent.Broadcast(error, socket);
	});
}

void UTCPServer::read(const asio::error_code& error, const size_t bytes_recvd, const socket_ptr& socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			if (!IsClosing)
			{
				disconnect_socket_after_error(error, socket);
			}
		});
		return;
	}
	FTcpMessage rbuffer;
	rbuffer.Size = bytes_recvd;
	rbuffer.RawData.SetNum(bytes_recvd);
	asio::buffer_copy(asio::buffer(rbuffer.RawData.GetData(), bytes_recvd),
	                  ListenerBuffer[socket]->data());

	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd, rbuffer, socket]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
		OnMessageReceived.Broadcast(rbuffer, socket);
	});

	consume_response_buffer(socket);
	asio::async_read(
		*socket, *ListenerBuffer[socket], asio::transfer_at_least(1),
		std::bind(&UTCPServer::read, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred, socket));
}

bool UTCPServerSsl::SendStrTo(const FString& Message, const FTCPSslSocket& SslSocket)
{
	if (!SslSocket.SmartPtr->next_layer().is_open() || Message.IsEmpty())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UTCPServerSsl::package_string, this, Message, SslSocket.SmartPtr));
	return true;
}

bool UTCPServerSsl::SendBufferTo(const TArray<uint8>& Buffer, const FTCPSslSocket& SslSocket)
{
	if (!SslSocket.SmartPtr->next_layer().is_open() || Buffer.Num() == 0)
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UTCPServerSsl::package_buffer, this, Buffer, SslSocket.SmartPtr));
	return true;
}

bool UTCPServerSsl::Open()
{
	if (TCP.acceptor.is_open())
	{
		return false;
	}

	asio::ip::tcp::endpoint endpoint(TcpProtocol == EProtocolType::V4
		                                 ? asio::ip::tcp::v4()
		                                 : asio::ip::tcp::v6(), TcpPort);
	TCP.acceptor.open(TcpProtocol == EProtocolType::V4
		                  ? asio::ip::tcp::v4()
		                  : asio::ip::tcp::v6(), ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	TCP.acceptor.set_option(asio::socket_base::reuse_address(true), ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	TCP.acceptor.bind(endpoint, ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}
	TCP.acceptor.listen(Backlog > 0
		                    ? Backlog
		                    : asio::socket_base::max_listen_connections,
	                    ErrorCode);
	if (ErrorCode)
	{
		FScopeLock Guard(&MutexError);
		OnError.Broadcast(ErrorCode);
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UTCPServerSsl::run_context_thread, this));
	return true;
}

void UTCPServerSsl::Close()
{
	IsClosing = true;
	if (!TCP.ssl_sockets.Num() == 0)
	{
		for (const ssl_socket_ptr& ssl_socket : TCP.ssl_sockets)
		{
			if (ssl_socket->next_layer().is_open())
			{
				FScopeLock Guard(&MutexError);
				ssl_socket->shutdown(ErrorCode);
				bool has_error = false;
				if (ErrorCode)
				{
					has_error = true;
					OnSocketDisconnected.Broadcast(ErrorCode, ssl_socket);
				}
				ssl_socket->next_layer().close(ErrorCode);
				if (ErrorCode)
				{
					has_error = true;
					OnSocketDisconnected.Broadcast(ErrorCode, ssl_socket);
				}
				if (!has_error)
				{
					OnSocketDisconnected.Broadcast(ErrorCode, ssl_socket);
				}
			}
		}
	}
	TCP.context.stop();
	if (TCP.ssl_sockets.Num() > 0)
	{
		TCP.ssl_sockets.Empty();
	}
	if (ListenerBuffer.Num() > 0)
	{
		ListenerBuffer.Empty();
	}
	if (TCP.acceptor.is_open())
	{
		FScopeLock Guard(&MutexError);
		TCP.acceptor.close(ErrorCode);
		if (ErrorCode)
		{
			OnError.Broadcast(ErrorCode);
		}
	}
	TCP.context.restart();
	TCP.acceptor = asio::ip::tcp::acceptor(TCP.context);
	OnClose.Broadcast();
	IsClosing = false;
}

void UTCPServerSsl::DisconnectSocket(const FTCPSslSocket& SslSocket)
{
	bool has_error = false;
	if (SslSocket.SmartPtr->next_layer().is_open())
	{
		FScopeLock Guard(&MutexError);
		SslSocket.SmartPtr->shutdown(ErrorCode);
		if (ErrorCode)
		{
			has_error = true;
			OnSocketDisconnected.Broadcast(ErrorCode, SslSocket.SmartPtr);
		}
		SslSocket.SmartPtr->next_layer().close(ErrorCode);
		if (ErrorCode)
		{
			has_error = true;
			OnSocketDisconnected.Broadcast(ErrorCode, SslSocket.SmartPtr);
		}
	}
	if (ListenerBuffer.Contains(SslSocket.SmartPtr))
	{
		ListenerBuffer.Remove(SslSocket.SmartPtr);
	}
	if (TCP.ssl_sockets.Contains(SslSocket.SmartPtr))
	{
		TCP.ssl_sockets.Remove(SslSocket.SmartPtr);
	}
	if (!has_error)
	{
		OnSocketDisconnected.Broadcast(FErrorCode(), SslSocket.SmartPtr);
	}
}

void UTCPServerSsl::disconnect_socket_after_error(const asio::error_code& error, const ssl_socket_ptr& ssl_socket)
{
	bool has_error = false;
	if (ssl_socket->next_layer().is_open())
	{
		FScopeLock Guard(&MutexError);
		ssl_socket->shutdown(ErrorCode);
		if (ErrorCode)
		{
			has_error = true;
			OnSocketDisconnected.Broadcast(ErrorCode, ssl_socket);
		}
		ssl_socket->next_layer().close(ErrorCode);
		if (ErrorCode)
		{
			has_error = true;
			OnSocketDisconnected.Broadcast(ErrorCode, ssl_socket);
		}
	}
	if (ListenerBuffer.Contains(ssl_socket))
	{
		ListenerBuffer.Remove(ssl_socket);
	}
	if (TCP.ssl_sockets.Contains(ssl_socket))
	{
		TCP.ssl_sockets.Remove(ssl_socket);
	}
	OnSocketDisconnected.Broadcast(error, ssl_socket);
}

void UTCPServerSsl::package_string(const FString& str, const ssl_socket_ptr& ssl_socket)
{
	FScopeLock Guard(&MutexBuffer);
	std::string packaged_str;
	if (!bSplitBuffer || str.Len() <= MaxSendBufferSize)
	{
		packaged_str = TCHAR_TO_UTF8(*str);
		asio::async_write(*ssl_socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPServerSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, ssl_socket));
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
		asio::async_write(*ssl_socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPServerSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, ssl_socket));
		string_offset += package_size;
	}
}

void UTCPServerSsl::package_buffer(const TArray<uint8>& buffer, const ssl_socket_ptr& ssl_socket)
{
	FScopeLock Guard(&MutexBuffer);
	if (!bSplitBuffer || buffer.Num() <= MaxSendBufferSize)
	{
		asio::async_write(*ssl_socket, asio::buffer(buffer.GetData(), buffer.Num()),
		                  std::bind(&UTCPServerSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, ssl_socket));
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
		asio::async_write(*ssl_socket, asio::buffer(sbuffer.GetData(), sbuffer.Num()),
		                  std::bind(&UTCPServerSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, ssl_socket));
		buffer_offset += package_size;
	}
}

void UTCPServerSsl::consume_response_buffer(const ssl_socket_ptr& socket)
{
	if (!ListenerBuffer.Contains(socket))
	{
		return;
	}
	const TSharedPtr<asio::streambuf> streambuf = ListenerBuffer[socket];
	const size_t size = streambuf->size();
	if (size > 0)
	{
		streambuf->consume(size);
	}
}

void UTCPServerSsl::run_context_thread()
{
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	ssl_socket_ptr ssl_conn_socket = MakeShared<asio::ssl::stream<asio::ip::tcp::socket>>(TCP.context, TCP.ssl_context);
	TCP.acceptor.async_accept(ssl_conn_socket->lowest_layer(),
	                          std::bind(&UTCPServerSsl::accept, this, asio::placeholders::error,
	                                    ssl_conn_socket));
	TCP.context.run();
	if (!IsClosing)
	{
		Close();
	}
}

void UTCPServerSsl::accept(const asio::error_code& error, ssl_socket_ptr& ssl_socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, ssl_socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			if (!IsClosing)
			{
				disconnect_socket_after_error(error, ssl_socket);
			}
		});
		if (TCP.acceptor.is_open())
		{
			ssl_socket_ptr ssl_conn_socket = MakeShared<asio::ssl::stream<asio::ip::tcp::socket>>(
				TCP.context, TCP.ssl_context);
			TCP.acceptor.async_accept(
				ssl_conn_socket->lowest_layer(),
				std::bind(&UTCPServerSsl::accept, this, asio::placeholders::error, ssl_conn_socket));
		}
		return;
	}
	
	if (TCP.ssl_sockets.Num() < Backlog)
	{
		ssl_socket->async_handshake(asio::ssl::stream_base::server,
		                            std::bind(&UTCPServerSsl::ssl_handshake, this,
		                                      asio::placeholders::error, ssl_socket));
	}
	else
	{
		FScopeLock Guard(&MutexError);
		AsyncTask(ENamedThreads::GameThread, [&, error, ssl_socket]()
		{
			if (!IsClosing)
			{
				disconnect_socket_after_error(error, ssl_socket);
			}
		});
	}

	if (TCP.acceptor.is_open())
	{
		ssl_socket_ptr ssl_conn_socket = MakeShared<asio::ssl::stream<asio::ip::tcp::socket>>(
			TCP.context, TCP.ssl_context);
		TCP.acceptor.async_accept(
			ssl_conn_socket->lowest_layer(),
			std::bind(&UTCPServerSsl::accept, this, asio::placeholders::error, ssl_conn_socket));
	}
}

void UTCPServerSsl::ssl_handshake(const asio::error_code& error, ssl_socket_ptr& ssl_socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, ssl_socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			if (!IsClosing)
			{
				disconnect_socket_after_error(error, ssl_socket);
			}
		});
		return;
	}

	TCP.ssl_sockets.Add(ssl_socket);
	TSharedPtr<asio::streambuf> response_buffer = MakeShared<asio::streambuf>();
	ListenerBuffer.Add(ssl_socket, response_buffer);
	asio::async_read(*ssl_socket, *ListenerBuffer[ssl_socket], asio::transfer_at_least(1),
	                 std::bind(&UTCPServerSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred, ssl_socket));

	AsyncTask(ENamedThreads::GameThread, [&, ssl_socket]()
	{
		OnSocketAccepted.Broadcast(ssl_socket);
	});
}

void UTCPServerSsl::write(const asio::error_code& error, const size_t bytes_sent, const ssl_socket_ptr& ssl_socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, ssl_socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnMessageSent.Broadcast(error, ssl_socket);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent, ssl_socket, error]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
		OnMessageSent.Broadcast(error, ssl_socket);
	});
}

void UTCPServerSsl::read(const asio::error_code& error, const size_t bytes_recvd, const ssl_socket_ptr& ssl_socket)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error, ssl_socket]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			if (!IsClosing)
			{
				disconnect_socket_after_error(error, ssl_socket);
			}
		});
		return;
	}
	FTcpMessage rbuffer;
	rbuffer.Size = bytes_recvd;
	rbuffer.RawData.SetNum(bytes_recvd);
	asio::buffer_copy(asio::buffer(rbuffer.RawData.GetData(), bytes_recvd),
	                  ListenerBuffer[ssl_socket]->data());

	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd, rbuffer, ssl_socket]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
		OnMessageReceived.Broadcast(rbuffer, ssl_socket);
	});

	consume_response_buffer(ssl_socket);
	asio::async_read(
		*ssl_socket, *ListenerBuffer[ssl_socket], asio::transfer_at_least(1),
		std::bind(&UTCPServerSsl::read, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred, ssl_socket));
}
