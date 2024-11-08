// Fill out your copyright notice in the Description page of Project Settings.

#include "TCP/TCPClient.h"

bool UTCPClient::send(const FString& message)
{
	if (!pool && !isConnected() && message.IsEmpty())
	{
		return false;
	}

	asio::post(*pool, [=]() { package_string(message); });
	return true;
}

bool UTCPClient::sendRaw(const TArray<uint8>& buffer)
{
	if (!pool && !isConnected() && buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(*pool, [=]() { package_buffer(buffer); });
	return true;
}

bool UTCPClient::asyncRead()
{
	if (!isConnected())
	{
		return false;
	}

	asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(2),
	                 std::bind(&UTCPClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred));
	return true;
}

bool UTCPClient::connect()
{
	if (pool.IsValid() && isConnected())
	{
		return false;
	}

	asio::post(*pool, std::bind(&UTCPClient::run_context_thread, this));
	return true;
}

void UTCPClient::close()
{
	tcp.context.stop();
	tcp.socket.shutdown(asio::ip::tcp::socket::shutdown_both, tcp.error_code);
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		if (tcp.error_code)
			OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
		tcp.socket.close(tcp.error_code);
		if (tcp.error_code)
			OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
		OnClose.Broadcast();
	});
}

void UTCPClient::package_string(const FString& str)
{
	mutexBuffer.lock();
	std::string packaged_str;
	if (!splitBuffer || str.Len() <= maxSendBufferSize)
	{
		packaged_str = TCHAR_TO_UTF8(*str);
		asio::async_write(tcp.socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		mutexBuffer.unlock();
		return;
	}

	size_t string_offset = 0;
	const size_t max_size = maxSendBufferSize;
	while (string_offset < str.Len())
	{
		size_t package_size = std::min(max_size, str.Len() - string_offset);
		FString strshrink = str.Mid(string_offset, package_size);
		packaged_str = TCHAR_TO_UTF8(*strshrink);
		asio::async_write(tcp.socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		string_offset += package_size;
	}
	mutexBuffer.unlock();
}

void UTCPClient::package_buffer(const TArray<uint8>& buffer)
{
	mutexBuffer.lock();
	if (!splitBuffer || buffer.Num() * sizeof(char) <= maxSendBufferSize)
	{
		asio::async_write(tcp.socket, asio::buffer(buffer.GetData(), buffer.Num() * sizeof(char)),
		                  std::bind(&UTCPClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		mutexBuffer.unlock();
		return;
	}

	size_t buffer_offset = 0;
	const size_t max_size = maxSendBufferSize - 1;
	while (buffer_offset < buffer.Num() * sizeof(char))
	{
		size_t package_size = std::min(max_size, (buffer.Num() * sizeof(char)) - buffer_offset);
		TArray<uint8> sbuffer;
		sbuffer.Append(buffer.GetData() + buffer_offset, package_size);
		if (sbuffer.Last() != '\0')
		{
			sbuffer.Add('\0');
		}
		asio::async_write(tcp.socket, asio::buffer(sbuffer.GetData(), sbuffer.Num() * sizeof(char)),
		                  std::bind(&UTCPClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
	}
	mutexBuffer.unlock();
}

void UTCPClient::run_context_thread()
{
	mutexIO.lock();
	while (tcp.attemps_fail <= maxAttemp)
	{
		if (ShouldStopContext) return;
		if (tcp.attemps_fail > 0)
		{
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				OnConnectionWillRetry.Broadcast(tcp.attemps_fail);
			});
		}
		tcp.error_code.clear();
		tcp.context.restart();
		tcp.resolver.async_resolve(TCHAR_TO_UTF8(*host), TCHAR_TO_UTF8(*service),
		                           std::bind(&UTCPClient::resolve, this, asio::placeholders::error,
		                                     asio::placeholders::results)
		);
		tcp.context.run();
		if (!tcp.error_code)
		{
			break;
		}
		tcp.attemps_fail++;
		std::this_thread::sleep_for(std::chrono::seconds(timeout));
	}
	consume_response_buffer();
	tcp.attemps_fail = 0;
	mutexIO.unlock();
}

void UTCPClient::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnError.Broadcast(error.value(), error.message().c_str());
				
		});
		return;
	}
	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.
	tcp.endpoints = endpoints;

	asio::async_connect(tcp.socket, tcp.endpoints,
	                    std::bind(&UTCPClient::conn, this, asio::placeholders::error)
	);
}

void UTCPClient::conn(const std::error_code& error)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}

	consume_response_buffer();
	asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
	                 std::bind(&UTCPClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnConnected.Broadcast();
	});
}

void UTCPClient::write(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnMessageSent.Broadcast(bytes_sent);
	});
}

void UTCPClient::read(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}

	FTcpMessage rbuffer;
	rbuffer.size = response_buffer.size();
	rbuffer.RawData.SetNum(rbuffer.size);
	asio::buffer_copy(asio::buffer(rbuffer.RawData.GetData(), rbuffer.size), response_buffer.data());
	const FTcpMessage buffer = rbuffer;
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnMessageReceived.Broadcast(bytes_recvd, buffer);
	});

	asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
	                 std::bind(&UTCPClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
}

bool UTCPClientSsl::send(const FString& message)
{
	if (!pool && !isConnected() && message.IsEmpty())
	{
		return false;
	}

	asio::post(*pool, [=]() { package_string(message); });
	return true;
}

bool UTCPClientSsl::sendRaw(const TArray<uint8>& buffer)
{
	if (!pool && !isConnected() && buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(*pool, [=]() { package_buffer(buffer); });
	return true;
}

bool UTCPClientSsl::asyncRead()
{
	if (!isConnected())
	{
		return false;
	}

	asio::async_read(tcp.ssl_socket, response_buffer, asio::transfer_at_least(2),
	                 std::bind(&UTCPClientSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred));
	return true;
}

bool UTCPClientSsl::connect()
{
	if (!pool && isConnected()) return false;

	asio::post(*pool, std::bind(&UTCPClientSsl::run_context_thread, this));
	return true;
}

void UTCPClientSsl::close()
{
	tcp.context.stop();
	if (ShouldStopContext)
	{
		tcp.ssl_socket.shutdown(tcp.error_code);
		AsyncTask(ENamedThreads::GameThread, [=]() 
		{
			if (tcp.error_code)
			{
				OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
				return;
			}
			OnClose.Broadcast();
		});
		return;
	}
	tcp.ssl_socket.async_shutdown([&](const std::error_code& error)
	{
		if (error)
		{
			tcp.error_code = error;
			const std::error_code err = error;
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				OnError.Broadcast(err.value(), err.message().c_str());
			});
			tcp.error_code.clear();
		}
		tcp.ssl_socket.lowest_layer().shutdown(
			asio::ip::tcp::socket::shutdown_both, tcp.error_code);
		if (tcp.error_code)
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
			});
		tcp.ssl_socket.lowest_layer().close(tcp.error_code);
		if (tcp.error_code)
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
			});
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnClose.Broadcast();
		});
	});
}

void UTCPClientSsl::package_string(const FString& str)
{
	mutexBuffer.lock();
	std::string packaged_str;
	if (!splitBuffer || str.Len() <= maxSendBufferSize)
	{
		packaged_str = TCHAR_TO_UTF8(*str);
		asio::async_write(tcp.ssl_socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		mutexBuffer.unlock();
		return;
	}

	size_t string_offset = 0;
	const size_t max_size = maxSendBufferSize;
	while (string_offset < str.Len())
	{
		size_t package_size = std::min(max_size, str.Len() - string_offset);
		FString strshrink = str.Mid(string_offset, package_size);
		packaged_str = TCHAR_TO_UTF8(*strshrink);
		asio::async_write(tcp.ssl_socket, asio::buffer(packaged_str.data(), packaged_str.size()),
		                  std::bind(&UTCPClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		string_offset += package_size;
	}
	mutexBuffer.unlock();
}

void UTCPClientSsl::package_buffer(const TArray<uint8>& buffer)
{
	mutexBuffer.lock();
	if (!splitBuffer || buffer.Num() * sizeof(char) <= maxSendBufferSize)
	{
		asio::async_write(tcp.ssl_socket, asio::buffer(buffer.GetData(), buffer.Num() * sizeof(char)),
		                  std::bind(&UTCPClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		mutexBuffer.unlock();
		return;
	}

	size_t buffer_offset = 0;
	const size_t max_size = maxSendBufferSize - 1;
	while (buffer_offset < buffer.Num() * sizeof(char))
	{
		size_t package_size = std::min(max_size, (buffer.Num() * sizeof(char)) - buffer_offset);
		TArray<uint8> sbuffer;
		sbuffer.Append(buffer.GetData() + buffer_offset, package_size);
		if (sbuffer.Last() != '\0')
		{
			sbuffer.Add('\0');
		}
		asio::async_write(tcp.ssl_socket, asio::buffer(sbuffer.GetData(), sbuffer.Num() * sizeof(char)),
		                  std::bind(&UTCPClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
	}
	mutexBuffer.unlock();
}

void UTCPClientSsl::run_context_thread()
{
	mutexIO.lock();
	while (tcp.attemps_fail <= maxAttemp)
	{
		if (ShouldStopContext) return;
		if (tcp.attemps_fail > 0)
		{
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				OnConnectionWillRetry.Broadcast(tcp.attemps_fail);
			});
		}
		tcp.error_code.clear();
		tcp.context.restart();
		tcp.resolver.async_resolve(TCHAR_TO_UTF8(*host), TCHAR_TO_UTF8(*service),
		                           std::bind(&UTCPClientSsl::resolve, this, asio::placeholders::error,
		                                     asio::placeholders::results)
		);
		tcp.context.run();
		if (!tcp.error_code)
		{
			break;
		}
		tcp.attemps_fail++;
		std::this_thread::sleep_for(std::chrono::seconds(timeout));
	}
	consume_response_buffer();
	tcp.attemps_fail = 0;
	mutexIO.unlock();
}

void UTCPClientSsl::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.
	tcp.endpoints = endpoints;

	asio::async_connect(tcp.ssl_socket.lowest_layer(), tcp.endpoints,
	                    std::bind(&UTCPClientSsl::conn, this, asio::placeholders::error)
	);
}

void UTCPClientSsl::conn(const std::error_code& error)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// The connection was successful;
	tcp.ssl_socket.async_handshake(asio::ssl::stream_base::client,
	                               std::bind(&UTCPClientSsl::ssl_handshake, this,
	                                         asio::placeholders::error));
}

void UTCPClientSsl::ssl_handshake(const std::error_code& error)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	consume_response_buffer();
	asio::async_read(
		tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
		std::bind(&UTCPClientSsl::read, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred));
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnConnected.Broadcast();
	});
}

void UTCPClientSsl::write(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnMessageSent.Broadcast(bytes_sent);
	});
}

void UTCPClientSsl::read(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}

	FTcpMessage rbuffer;
	rbuffer.size = response_buffer.size();
	rbuffer.RawData.SetNum(rbuffer.size);
	asio::buffer_copy(asio::buffer(rbuffer.RawData.GetData(), rbuffer.size), response_buffer.data());
	const FTcpMessage buffer = rbuffer;
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnMessageReceived.Broadcast(bytes_recvd, buffer);
	});

	asio::async_read(tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
	                 std::bind(&UTCPClientSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
}
