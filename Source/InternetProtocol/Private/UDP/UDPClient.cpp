// Fill out your copyright notice in the Description page of Project Settings.

#include "UDP/UDPClient.h"

bool UUDPClient::send(const FString& message)
{
	if (!pool && !isConnected() && !message.IsEmpty())
	{
		return false;
	}

	asio::post(*pool, [this, message]() { package_string(message); });
	return true;
}

bool UUDPClient::sendRaw(const TArray<uint8>& buffer)
{
	if (!pool && !isConnected() && buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(*pool, [this, buffer]() { package_buffer(buffer); });
	return true;
}

bool UUDPClient::asyncRead()
{
	if (!isConnected())
	{
		return false;
	}

	udp.socket.async_receive_from(asio::buffer(rbuffer.RawData.GetData(), rbuffer.RawData.Num()), udp.endpoints,
								  std::bind(&UUDPClient::receive_from, this, asio::placeholders::error,
											asio::placeholders::bytes_transferred)
	);
	return true;
}

bool UUDPClient::connect()
{
	if (!pool && isConnected())
	{
		return false;
	}

	asio::post(*pool, std::bind(&UUDPClient::run_context_thread, this));
	return true;
}

void UUDPClient::close()
{
	udp.context.stop();
	udp.socket.close(udp.error_code);
	pool->join();
	if (udp.error_code)
	{
		OnError.Broadcast(udp.error_code.value(), udp.error_code.message().c_str());
	}
	OnClose.Broadcast();
}

void UUDPClient::package_string(const FString& str)
{
	mutexBuffer.lock();
	std::string packaged_str;
	if (!splitBuffer || str.Len() <= maxSendBufferSize)
	{
		packaged_str = TCHAR_TO_UTF8(*str);
		udp.socket.async_send_to(asio::buffer(packaged_str.data(), packaged_str.size()), udp.endpoints,
		                         std::bind(&UUDPClient::send_to, this, asio::placeholders::error,
		                                   asio::placeholders::bytes_transferred));
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
		udp.socket.async_send_to(asio::buffer(packaged_str.data(), packaged_str.size()), udp.endpoints,
		                         std::bind(&UUDPClient::send_to, this, asio::placeholders::error,
		                                   asio::placeholders::bytes_transferred));
		string_offset += package_size;
	}
	mutexBuffer.unlock();
}

void UUDPClient::package_buffer(const TArray<uint8>& buffer)
{
	mutexBuffer.lock();
	if (!splitBuffer || buffer.Num() <= maxSendBufferSize)
	{
		udp.socket.async_send_to(asio::buffer(buffer.GetData(), buffer.Num() * sizeof(char)), udp.endpoints,
		                         std::bind(&UUDPClient::send_to, this, asio::placeholders::error,
		                                   asio::placeholders::bytes_transferred)
		);
		mutexBuffer.unlock();
		return;
	}

	size_t buffer_offset = 0;
	const size_t max_size = maxSendBufferSize - 1;
	while (buffer_offset < buffer.Num() * sizeof(char))
	{
		size_t package_size = std::min(max_size, buffer.Num() - buffer_offset);
		TArray<uint8> sbuffer;
		sbuffer.Append(buffer.GetData() + buffer_offset, package_size);
		udp.socket.async_send_to(asio::buffer(sbuffer.GetData(), sbuffer.Num() * sizeof(char)), udp.endpoints,
		                         std::bind(&UUDPClient::send_to, this, asio::placeholders::error,
		                                   asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
	}
	mutexBuffer.unlock();
}

void UUDPClient::run_context_thread()
{
	mutexIO.lock();
	while (udp.attemps_fail <= maxAttemp)
	{
		if (udp.attemps_fail > 0)
		{
			OnConnectionWillRetry.Broadcast(udp.attemps_fail);
		}
		udp.error_code.clear();
		udp.context.restart();
		udp.resolver.async_resolve(asio::ip::udp::v4(), TCHAR_TO_UTF8(*host), TCHAR_TO_UTF8(*service),
		                           std::bind(&UUDPClient::resolve, this, asio::placeholders::error,
		                                     asio::placeholders::results)
		);
		udp.context.run();
		if (!udp.error_code)
		{
			break;
		}
		udp.attemps_fail++;
		std::this_thread::sleep_for(std::chrono::seconds(timeout));
	}
	consume_receive_buffer();
	udp.attemps_fail = 0;
	mutexIO.unlock();
}

void UUDPClient::resolve(const std::error_code& error, const asio::ip::udp::resolver::results_type &results)
{
	if (error)
	{
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	udp.endpoints = *results;
	udp.socket.async_connect(udp.endpoints,
	                         std::bind(&UUDPClient::conn, this, asio::placeholders::error)
	);
}

void UUDPClient::conn(const std::error_code& error)
{
	if (error)
	{
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	consume_receive_buffer();
	udp.socket.async_receive_from(asio::buffer(rbuffer.RawData.GetData(), rbuffer.RawData.Num()), udp.endpoints,
	                              std::bind(&UUDPClient::receive_from, this, asio::placeholders::error,
	                                        asio::placeholders::bytes_transferred)
	);

	OnConnected.Broadcast();
}

void UUDPClient::send_to(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	OnMessageSent.Broadcast(bytes_sent);
}

void UUDPClient::receive_from(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	udp.attemps_fail = 0;
	rbuffer.size = bytes_recvd;
	rbuffer.RawData.SetNum(bytes_recvd);
	OnMessageReceived.Broadcast(bytes_recvd, rbuffer);

	consume_receive_buffer();
	udp.socket.async_receive_from(asio::buffer(rbuffer.RawData.GetData(), rbuffer.RawData.Num()), udp.endpoints,
	                              std::bind(&UUDPClient::receive_from, this, asio::placeholders::error,
	                                        asio::placeholders::bytes_transferred)
	);
}
