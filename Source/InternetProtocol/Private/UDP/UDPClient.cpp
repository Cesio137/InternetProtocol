// Fill out your copyright notice in the Description page of Project Settings.


#include "UDP/UDPClient.h"

void UUDPClient::send(const FString& message)
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

void UUDPClient::connect()
{
	if (!pool)
		return;

	asio::post(*pool, [this]() {
		runContextThread();
	});
}

void UUDPClient::runContextThread()
{
	mutexIO.lock();
	udp.context.restart();
	udp.resolver.async_resolve(asio::ip::udp::v4(), TCHAR_TO_UTF8(*host), TCHAR_TO_UTF8(*service),
		std::bind(&UUDPClient::resolve, this, asio::placeholders::error, asio::placeholders::results)
	);
	udp.context.run();
	mutexIO.unlock();
}

void UUDPClient::resolve(std::error_code error, asio::ip::udp::resolver::results_type results)
{
	if (error) {
		OnConnectionError.Broadcast( error.value(), UTF8_TO_TCHAR(error.message().c_str()) );
		return;
	}
	udp.endpoints = *results;
	udp.socket.async_connect(udp.endpoints,
		std::bind(&UUDPClient::conn, this, asio::placeholders::error)
	);
}

void UUDPClient::conn(std::error_code error)
{
	if (error) {
		OnConnectionError.Broadcast( error.value(), UTF8_TO_TCHAR(error.message().c_str()) );
		return;
	}

	udp.socket.async_receive_from(asio::buffer(rbuffer.message.GetData(), 1024), udp.endpoints,
		std::bind(&UUDPClient::receive_from, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
	);

	OnConnected.Broadcast();
}

void UUDPClient::package_buffer(const TArray<char>& buffer)
{
	mutexBuffer.lock();
	if (buffer.Num() * sizeof(char) <= 1024) {
		udp.socket.async_send_to(asio::buffer(buffer.GetData(), buffer.Num() * sizeof(char)), udp.endpoints,
			std::bind(&UUDPClient::send_to, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
		);
		mutexBuffer.unlock();
		return;
	}

	size_t buffer_offset = 0;
	const size_t max_size = 1023;
	while (buffer_offset < buffer.Num() * sizeof(char)) {
		size_t package_size = std::min(max_size, (buffer.Num() * sizeof(char)) - buffer_offset);
		TArray<char> sbuffer;
		sbuffer.Append(buffer.GetData() + buffer_offset, package_size);
		if (sbuffer.Last() != '\0')
			sbuffer.Add('\0');
		udp.socket.async_send_to(asio::buffer(sbuffer.GetData(), sbuffer.Num() * sizeof(char)), udp.endpoints,
			std::bind(&UUDPClient::send_to, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
	}
	mutexBuffer.unlock();
}

void UUDPClient::send_to(std::error_code error, std::size_t bytes_sent)
{
	if (error) {
		OnMessageSentError.Broadcast( error.value(), UTF8_TO_TCHAR(error.message().c_str()) );
		return;
	}
	
	OnMessageSent.Broadcast(bytes_sent);
}

void UUDPClient::receive_from(std::error_code error, std::size_t bytes_recvd)
{
	if (error) {
		OnMessageReceivedError.Broadcast( error.value(), UTF8_TO_TCHAR(error.message().c_str()) );
		return;
	}
	rbuffer.size = bytes_recvd;
	OnMessageReceived.Broadcast(bytes_recvd, rbuffer);

	udp.socket.async_receive_from(asio::buffer(rbuffer.message.GetData(), 1024), udp.endpoints,
		std::bind(&UUDPClient::receive_from, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
	);
}
