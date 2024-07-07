// Fill out your copyright notice in the Description page of Project Settings.


#include "TCP/TCPClient.h"

void UTCPClient::send(const FString& message)
{
	if (!pool || !isConnected() || message.IsEmpty())
		return;
            
	asio::post(*pool, [this, message]() {
		FTCHARToUTF8 utf8Str(*message);
		TArray<char> data((char*)utf8Str.Get(), utf8Str.Length());
		if (data.Last() == '\0')
			data.Add('\0');
		package_buffer(data);
	});
}

void UTCPClient::connect()
{
	if (!pool)
		return;

	asio::post(*pool, [this]() {
		runContextThread();
	});
}

void UTCPClient::runContextThread()
{
	mutexIO.lock();
	tcp.context.restart();
	tcp.resolver.async_resolve(TCHAR_TO_UTF8(*host), TCHAR_TO_UTF8(*service),
		std::bind(&UTCPClient::resolve, this, asio::placeholders::error, asio::placeholders::results)
	);
	tcp.context.run();
	if (tcp.error_code && maxAttemp > 0 && timeout > 0) {
		uint8 attemp = 0;
		while(attemp < maxAttemp) {
			OnConnectionWillRetry.Broadcast(attemp + 1);
			tcp.error_code.clear();
			tcp.context.restart();
			asio::steady_timer timer(tcp.context, asio::chrono::seconds(timeout));
			timer.async_wait([this](const std::error_code& error) {
				if (error) {
					OnConnectionError.Broadcast(error.value(), UTF8_TO_TCHAR(error.message().c_str()));
				}
				tcp.resolver.async_resolve(TCHAR_TO_UTF8(*host), TCHAR_TO_UTF8(*service),
					std::bind(&UTCPClient::resolve, this, asio::placeholders::error, asio::placeholders::results)
				);
			});
			tcp.context.run();
			attemp += 1;
			if(!tcp.error_code)
				break;
		}
	}
	mutexIO.unlock();
}

void UTCPClient::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error) {
		tcp.error_code = error;
		OnConnectionError.Broadcast(error.value(), UTF8_TO_TCHAR(error.message().c_str()));
		return;
	}
	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.
	tcp.endpoints = endpoints;

	asio::async_connect(tcp.socket, tcp.endpoints,
		std::bind(&UTCPClient::conn, this, asio::placeholders::error)
	);
}

void UTCPClient::conn(std::error_code error)
{
	if (error) {
		tcp.error_code = error;
		OnConnectionError.Broadcast(error.value(), UTF8_TO_TCHAR(error.message().c_str()));
		return;
	}
	// The connection was successful;
	tcp.socket.set_option(asio::socket_base::send_buffer_size(1400));
	tcp.socket.set_option(asio::socket_base::receive_buffer_size(rbuffer.message.Num()));

	asio::async_read(tcp.socket, asio::buffer(rbuffer.message.GetData(), rbuffer.message.Num()), asio::transfer_at_least(1),
		std::bind(&UTCPClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
	);

	OnConnected.Broadcast();
}

void UTCPClient::package_buffer(const TArray<char>& buffer)
{
	mutexBuffer.lock();
	if (!splitBuffer || buffer.Num() * sizeof(char) <= maxBufferSize) {
		asio::async_write(tcp.socket, asio::buffer(buffer.GetData(), buffer.Num() * sizeof(char)),
			std::bind(&UTCPClient::write, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
		);
		mutexBuffer.unlock();
		return;
	}

	size_t buffer_offset = 0;
	const size_t max_size = maxBufferSize - 1;
	while (buffer_offset < buffer.Num() * sizeof(char)) {
		size_t package_size = std::min(max_size, (buffer.Num() * sizeof(char)) - buffer_offset);
		TArray<char> sbuffer;
		sbuffer.Append(buffer.GetData() + buffer_offset, package_size);
		if (sbuffer.Last() != '\0')
			sbuffer.Add('\0');
		asio::async_write(tcp.socket, asio::buffer(sbuffer.GetData(), sbuffer.Num() * sizeof(char)),
			std::bind(&UTCPClient::write, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
	}
	mutexBuffer.unlock();
}

void UTCPClient::write(std::error_code error, std::size_t bytes_sent)
{
	if (error) {
		OnMessageSentError.Broadcast(error.value(), UTF8_TO_TCHAR(error.message().c_str()));
		return;
	}
	OnMessageSent.Broadcast(bytes_sent);
}

void UTCPClient::read(std::error_code error, std::size_t bytes_recvd)
{
	if (error) {
		OnMessageReceivedError.Broadcast(error.value(), UTF8_TO_TCHAR(error.message().c_str()));
		return;
	}
	rbuffer.size = bytes_recvd;
	OnMessageReceived.Broadcast(bytes_recvd, rbuffer);

	asio::async_read(tcp.socket, asio::buffer(rbuffer.message.GetData(), rbuffer.message.Num()), asio::transfer_at_least(1),
		std::bind(&UTCPClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
	);
}
