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

#include "Websockets/WebsocketClient.h"

bool UWebsocketClient::Send(const FString& message)
{
	if (!IsConnected() && message.IsEmpty())
	{
		return false;
	}
	
	asio::post(*ThreadPool, std::bind(&UWebsocketClient::post_string, this, message));
	return true;
}

bool UWebsocketClient::SendRaw(const TArray<uint8> buffer)
{
	if (!IsConnected() && buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(*ThreadPool, std::bind(&UWebsocketClient::post_buffer, this, EOpcode::BINARY_FRAME, buffer));
	return true;
}

bool UWebsocketClient::SendPing()
{
	if (!IsConnected())
	{
		return false;
	}
	
	TArray<uint8> ping_buffer = {'p', 'i', 'n', 'g', '\0'};
	asio::post(*ThreadPool, std::bind(&UWebsocketClient::post_buffer, this, EOpcode::PING, ping_buffer));
	return true;
}

bool UWebsocketClient::AsyncRead()
{
	if (!IsConnected())
	{
		return false;
	}

	asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(2),
	                 std::bind(&UWebsocketClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred));
	return true;
}

bool UWebsocketClient::Connect()
{
	if (!IsConnected())
	{
		return false;
	}
	
	asio::post(*ThreadPool, std::bind(&UWebsocketClient::run_context_thread, this));
	return true;
}

void UWebsocketClient::Close()
{
	asio::error_code ec_shutdown;
	asio::error_code ec_close;
	TCP.context.stop();
	TCP.socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec_shutdown);
	TCP.socket.close(ec_close);
	if (ShouldStopContext) return;
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

void UWebsocketClient::post_string(const FString& str)
{
	MutexBuffer.Lock();
	SDataFrame.opcode = EOpcode::TEXT_FRAME;
	package_string(str);
	MutexBuffer.Unlock();
}

void UWebsocketClient::post_buffer(EOpcode opcode, const TArray<uint8>& buffer)
{
	MutexBuffer.Lock();
	SDataFrame.opcode = opcode;
	if (opcode == EOpcode::BINARY_FRAME)
	{
		package_buffer(buffer);
	}
	else if (opcode == EOpcode::PING || opcode == EOpcode::PONG)
	{
		std::vector<uint8> p_buffer(buffer.GetData(), buffer.GetData() + buffer.Num());
		encode_buffer_payload(p_buffer);
		asio::async_write(TCP.socket, asio::buffer(p_buffer.data(), p_buffer.size()),
		                  std::bind(&UWebsocketClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred));
	}
	MutexBuffer.Unlock();
}

void UWebsocketClient::package_string(const FString& str)
{
	std::string payload;
	if (!SplitBuffer || str.Len() /*+ get_frame_encode_size(str.size())*/ <= MaxSendBufferSize)
	{
		SDataFrame.fin = true;
		payload = TCHAR_TO_UTF8(*str);
		payload = encode_string_payload(payload);
		asio::async_write(TCP.socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWebsocketClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		return;
	}

	SDataFrame.fin = true;
	size_t string_offset = 0;
	const size_t max_size = MaxSendBufferSize - get_frame_encode_size(str.Len());
	while (string_offset < str.Len())
	{
		SDataFrame.fin = string_offset < str.Len();
		size_t package_size = std::min(max_size, str.Len() - string_offset);
		FString strshrink = str.Mid(string_offset, package_size);
		payload = TCHAR_TO_UTF8(*strshrink);
		payload = encode_string_payload(payload);
		asio::async_write(TCP.socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWebsocketClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		string_offset += package_size;
		if (SDataFrame.opcode != EOpcode::FRAME_CON)
		{
			SDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::string UWebsocketClient::encode_string_payload(const std::string& payload)
{
	std::string string_buffer;
	uint64 payload_length = payload.size();

	// FIN, RSV, Opcode
	uint8 byte1 = SDataFrame.fin ? 0x80 : 0x00;
	byte1 |= SDataFrame.rsv1 ? (uint8)ERSV::RSV1 : 0x00;
	byte1 |= SDataFrame.rsv2 ? (uint8)ERSV::RSV2 : 0x00;
	byte1 |= SDataFrame.rsv3 ? (uint8)ERSV::RSV3 : 0x00;
	byte1 |= (uint8)SDataFrame.opcode & 0x0F;
	string_buffer.push_back(byte1);

	// Mask and payload size
	uint8 byte2 = SDataFrame.mask ? 0x80 : 0x00;
	if (payload_length <= 125)
	{
		byte2 |= payload_length;
		string_buffer.push_back(byte2);
	}
	else if (payload_length <= 65535)
	{
		byte2 |= 126;
		string_buffer.push_back(byte2);
		string_buffer.push_back((payload_length >> 8) & 0xFF);
		string_buffer.push_back(payload_length & 0xFF);
	}
	else
	{
		byte2 |= 127;
		string_buffer.push_back(byte2);
		for (int i = 7; i >= 0; --i)
		{
			string_buffer.push_back((payload_length >> (8 * i)) & 0xFF);
		}
	}

	std::array<uint8, 4> masking_key;
	if (SDataFrame.mask)
	{
		masking_key = mask_gen();
		for (uint8 key : masking_key)
		{
			string_buffer.push_back(static_cast<uint8_t>(key));
		}
	}

	// payload data and mask
	for (size_t i = 0; i < payload.size(); ++i)
	{
		if (SDataFrame.mask)
		{
			string_buffer.push_back(payload[i] ^ static_cast<uint8_t>(masking_key[i % 4]));
		}
		else
		{
			string_buffer.push_back(payload[i]);
		}
	}

	return string_buffer;
}

void UWebsocketClient::package_buffer(const TArray<uint8>& buffer)
{
	std::vector<uint8> payload;
	if (!SplitBuffer || buffer.Num() + get_frame_encode_size(buffer.Num()) <= MaxSendBufferSize)
	{
		payload.assign(buffer.GetData(), buffer.GetData() + buffer.Num());
		SDataFrame.fin = true;
		encode_buffer_payload(payload);
		asio::async_write(TCP.socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWebsocketClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		return;
	}

	SDataFrame.fin = false;
	size_t buffer_offset = 0;
	const size_t max_size = MaxSendBufferSize - get_frame_encode_size(buffer.Num());
	while (buffer_offset < buffer.Num())
	{
		SDataFrame.fin = buffer_offset < buffer.Num();
		size_t package_size = std::min(max_size, buffer.Num() - buffer_offset);
		payload.assign(buffer.GetData() + buffer_offset, buffer.GetData() + buffer_offset + package_size);
		encode_buffer_payload(payload);
		asio::async_write(TCP.socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWebsocketClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
		if (SDataFrame.opcode != EOpcode::FRAME_CON)
		{
			SDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::vector<uint8> UWebsocketClient::encode_buffer_payload(const std::vector<uint8>& payload)
{
	std::vector<uint8> buffer;
	uint64 payload_length = payload.size();

	// FIN, RSV, Opcode
	uint8 byte1 = SDataFrame.fin ? 0x80 : 0x00;
	byte1 |= SDataFrame.rsv1 ? (uint8)ERSV::RSV1 : 0x00;
	byte1 |= SDataFrame.rsv2 ? (uint8)ERSV::RSV2 : 0x00;
	byte1 |= SDataFrame.rsv3 ? (uint8)ERSV::RSV3 : 0x00;
	byte1 |= (uint8)SDataFrame.opcode & 0x0F;
	buffer.push_back(byte1);

	// Mask and payload size
	uint8 byte2 = SDataFrame.mask ? 0x80 : 0x00;
	if (payload_length <= 125)
	{
		byte2 |= payload_length;
		buffer.push_back(byte2);
	}
	else if (payload_length <= 65535)
	{
		byte2 |= 126;
		buffer.push_back(byte2);
		buffer.push_back((payload_length >> 8) & 0xFF);
		buffer.push_back(payload_length & 0xFF);
	}
	else
	{
		byte2 |= 127;
		buffer.push_back(byte2);
		for (int i = 7; i >= 0; --i)
		{
			buffer.push_back((payload_length >> (8 * i)) & 0xFF);
		}
	}

	std::array<uint8, 4> masking_key;
	if (SDataFrame.mask)
	{
		masking_key = mask_gen();
		for (uint8 key : masking_key)
		{
			buffer.push_back(key);
		}
	}

	// payload data and mask
	for (size_t i = 0; i < payload.size(); ++i)
	{
		if (SDataFrame.mask)
		{
			buffer.push_back(payload[i] ^ masking_key[i % 4]);
		}
		else
		{
			buffer.push_back(payload[i]);
		}
	}

	return buffer;
}

size_t UWebsocketClient::get_frame_encode_size(const size_t buffer_size)
{
	size_t size = 2;
	if (buffer_size <= 125)
	{
		size += 0;
	}
	else if (buffer_size <= 65535)
	{
		size += 2;
	}
	else
	{
		size += 8;
	}

	if (SDataFrame.mask)
	{
		size += 4;
	}

	return size;
}

std::array<uint8, 4> UWebsocketClient::mask_gen()
{
	std::array<uint8, 4> maskKey;
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 255);

	for (uint8& byte : maskKey)
	{
		byte = dis(gen);
	}

	return maskKey;
}

bool UWebsocketClient::decode_payload(FWsMessage& data_frame)
{
	if (ResponseBuffer.size() < 2)
	{
		return false;
	}

	size_t size = ResponseBuffer.size();
	std::vector<uint8> encoded_buffer;
	encoded_buffer.resize(size);
	asio::buffer_copy(asio::buffer(encoded_buffer, encoded_buffer.size()), ResponseBuffer.data());
	size_t pos = 0;

	// FIN, RSV, Opcode
	uint8 byte1 = encoded_buffer[pos++];
	data_frame.DataFrame.fin = byte1 & 0x80;
	data_frame.DataFrame.rsv1 = byte1 & 0x80;
	data_frame.DataFrame.rsv2 = byte1 & 0x40;
	data_frame.DataFrame.rsv3 = (uint8_t)byte1 & 0x10;
	data_frame.DataFrame.opcode = (EOpcode)((uint8_t)byte1 & 0x0F);

	// Mask and payload length
	uint8 byte2 = encoded_buffer[pos++];
	data_frame.DataFrame.mask = byte2 & 0x80;
	uint64 payload_length = byte2 & 0x7F;
	if (payload_length == 126)
	{
		if (encoded_buffer.size() < pos + 2)
		{
			return false;
		}
		payload_length = static_cast<uint64>((encoded_buffer[pos] << 8) | encoded_buffer[pos + 1]);
		pos += 2;
	}
	else if (payload_length == 127)
	{
		if (encoded_buffer.size() < pos + 8)
		{
			return false;
		}
		payload_length = 0;
		for (int i = 0; i < 8; ++i)
		{
			payload_length = (payload_length << 8) | encoded_buffer[pos + i];
		}
		pos += 8;
	}
	data_frame.DataFrame.length = payload_length;
	// Masking key
	if (data_frame.DataFrame.mask)
	{
		if (encoded_buffer.size() < pos + 4)
		{
			return false;
		}
		for (int i = 0; i < 4; ++i)
		{
			data_frame.DataFrame.masking_key.Add(encoded_buffer[pos++]);
		}
	}
	// Payload data
	if (encoded_buffer.size() < pos + payload_length)
	{
		return false;
	}
	//data_frame.Payload.reserve(payload_length);
	for (size_t i = 0; i < payload_length; ++i)
	{
		uint8 byte = encoded_buffer[pos++];
		if (data_frame.DataFrame.mask)
		{
			byte ^= data_frame.DataFrame.masking_key[i % 4];
		}
		data_frame.Payload.Add(byte);
	}
	return true;
}

void UWebsocketClient::run_context_thread()
{
	MutexIO.Lock();
	while (TCP.attemps_fail <= MaxAttemp && !ShouldStopContext)
	{
		if (TCP.attemps_fail > 0)
		{
			OnConnectionWillRetry.Broadcast(TCP.attemps_fail);
		}
		TCP.error_code.clear();
		TCP.context.restart();
		TCP.resolver.async_resolve(TCHAR_TO_UTF8(*Host), TCHAR_TO_UTF8(*Service),
		                           std::bind(&UWebsocketClient::resolve, this, asio::placeholders::error,
		                                     asio::placeholders::endpoint));
		TCP.context.run();
		if (!TCP.error_code)
		{
			break;
		}
		TCP.attemps_fail++;
		std::this_thread::sleep_for(std::chrono::seconds(Timeout));
	}
	consume_response_buffer();
	TCP.attemps_fail = 0;
	MutexIO.Unlock();
}

void UWebsocketClient::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.
	TCP.endpoints = endpoints;

	asio::async_connect(TCP.socket, TCP.endpoints,
	                    std::bind(&UWebsocketClient::conn, this, asio::placeholders::error)
	);
}

void UWebsocketClient::conn(const std::error_code& error)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	// The connection was successful;
	FString request;
	request = "GET /" + Handshake.path + " HTTP/" + Handshake.version + "\r\n";
	request += "Host: " + Host + "\r\n";
	request += "Upgrade: websocket\r\n";
	request += "Connection: Upgrade\r\n";
	request += "Sec-WebSocket-Key: " + Handshake.Sec_WebSocket_Key + "\r\n";
	request += "Origin: " + Handshake.origin + "\r\n";
	request += "Sec-WebSocket-Protocol: " + Handshake.Sec_WebSocket_Protocol + "\r\n";
	request += "Sec-WebSocket-Version: " + Handshake.Sec_Websocket_Version + "\r\n";
	request += "\r\n";
	std::ostream request_stream(&RequestBuffer);
	request_stream << TCHAR_TO_UTF8(*request);
	asio::async_write(TCP.socket, RequestBuffer,
	                  std::bind(&UWebsocketClient::write_handshake, this, asio::placeholders::error,
	                            asio::placeholders::bytes_transferred));
}

void UWebsocketClient::write_handshake(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	// Read the response status line. The response_ streambuf will
	// automatically grow to accommodate the entire line. The growth may be
	// limited by passing a maximum size to the streambuf constructor.
	asio::async_read_until(TCP.socket, ResponseBuffer, "\r\n",
	                       std::bind(&UWebsocketClient::read_handshake, this, asio::placeholders::error,
	                                 bytes_sent, asio::placeholders::bytes_transferred));
}

void UWebsocketClient::read_handshake(const std::error_code& error, const size_t bytes_sent,
                                      const size_t bytes_recvd)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	// Check that response is OK.
	std::istream response_stream(&ResponseBuffer);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		OnError.Broadcast(-1, "Invalid response.");
		Close();
		return;
	}
	if (status_code != 101)
	{
		OnError.Broadcast(status_code, "Invalid status code.");
		Close();
		return;
	}

	asio::async_read_until(TCP.socket, ResponseBuffer, "\r\n\r\n",
	                       std::bind(&UWebsocketClient::consume_header_buffer, this, asio::placeholders::error));
}

void UWebsocketClient::consume_header_buffer(const std::error_code& error)
{
	consume_response_buffer();
	asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UWebsocketClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred));

	// The connection was successful
	OnConnected.Broadcast();
}

void UWebsocketClient::write(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	OnMessageSent.Broadcast(bytes_sent);
}

void UWebsocketClient::read(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	FWsMessage rDataFrame;
	if (!decode_payload(rDataFrame))
	{
		ResponseBuffer.consume(ResponseBuffer.size());
		asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
		                 std::bind(&UWebsocketClient::read, this, asio::placeholders::error,
		                           asio::placeholders::bytes_transferred)
		);
		return;
	}

	if (rDataFrame.DataFrame.opcode == EOpcode::PING)
	{
		TArray<uint8> pong_buffer = {'p', 'o', 'n', 'g', '\0'};
		post_buffer(EOpcode::PONG, pong_buffer);
	}
	else if (rDataFrame.DataFrame.opcode == EOpcode::PONG)
	{
		OnPongReceived.Broadcast();
	}
	else if (rDataFrame.DataFrame.opcode == EOpcode::CONNECTION_CLOSE)
	{
		OnCloseNotify.Broadcast();
	}
	else
	{
		OnMessageReceived.Broadcast(rDataFrame);
	}

	consume_response_buffer();
	asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UWebsocketClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
}

bool UWebsocketClientSsl::Send(const FString& message)
{
	if (!IsConnected() && message.IsEmpty())
	{
		return false;
	}
	
	asio::post(*ThreadPool, std::bind(&UWebsocketClientSsl::post_string, this, message));
	return true;
}

bool UWebsocketClientSsl::SendRaw(const TArray<uint8> buffer)
{
	if (!IsConnected() && buffer.Num() <= 0)
	{
		return false;
	}
	
	asio::post(*ThreadPool, std::bind(&UWebsocketClientSsl::post_buffer, this, EOpcode::BINARY_FRAME, buffer));
	return true;
}

bool UWebsocketClientSsl::SendPing()
{
	if (!IsConnected())
	{
		return false;
	}
	
	TArray<uint8> ping_buffer = {'p', 'i', 'n', 'g', '\0'};
	asio::post(*ThreadPool, std::bind(&UWebsocketClientSsl::post_buffer, this, EOpcode::PONG, ping_buffer));
	return true;
}

bool UWebsocketClientSsl::AsyncRead()
{
	if (!IsConnected())
	{
		return false;
	}

	asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(2),
	                 std::bind(&UWebsocketClientSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred));
	return true;
}

bool UWebsocketClientSsl::Connect()
{
	if (!IsConnected())
	{
		return false;
	}

	asio::post(*ThreadPool, std::bind(&UWebsocketClientSsl::run_context_thread, this));
	return true;
}

void UWebsocketClientSsl::Close()
{
	TCP.context.stop();
	asio::error_code ec_shutdown;
	asio::error_code ec_close;
	TCP.ssl_socket.shutdown(ec_shutdown);
	TCP.ssl_socket.lowest_layer().close(ec_close);
	if (ShouldStopContext) return;
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

void UWebsocketClientSsl::post_string(const FString& str)
{
	MutexBuffer.Lock();
	SDataFrame.opcode = EOpcode::TEXT_FRAME;
	package_string(str);
	MutexBuffer.Unlock();
}

void UWebsocketClientSsl::post_buffer(EOpcode opcode, const TArray<uint8>& buffer)
{
	MutexBuffer.Lock();
	SDataFrame.opcode = opcode;
	if (opcode == EOpcode::BINARY_FRAME)
	{
		package_buffer(buffer);
	}
	else if (opcode == EOpcode::PING || opcode == EOpcode::PONG)
	{
		std::vector<uint8> p_buffer(buffer.GetData(), buffer.GetData() + buffer.Num());
		encode_buffer_payload(p_buffer);
		asio::async_write(TCP.ssl_socket, asio::buffer(p_buffer.data(), p_buffer.size()),
		                  std::bind(&UWebsocketClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred));
	}
	MutexBuffer.Unlock();
}

void UWebsocketClientSsl::package_string(const FString& str)
{
	std::string payload;
	if (!SplitBuffer || str.Len() /*+ get_frame_encode_size(str.size())*/ <= MaxSendBufferSize)
	{
		SDataFrame.fin = true;
		payload = TCHAR_TO_UTF8(*str);
		payload = encode_string_payload(payload);
		asio::async_write(TCP.ssl_socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWebsocketClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		return;
	}

	SDataFrame.fin = true;
	size_t string_offset = 0;
	const size_t max_size = MaxSendBufferSize - get_frame_encode_size(str.Len());
	while (string_offset < str.Len())
	{
		SDataFrame.fin = string_offset < str.Len();
		size_t package_size = std::min(max_size, str.Len() - string_offset);
		FString strshrink = str.Mid(string_offset, package_size);
		payload = TCHAR_TO_UTF8(*strshrink);
		payload = encode_string_payload(payload);
		asio::async_write(TCP.ssl_socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWebsocketClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		string_offset += package_size;
		if (SDataFrame.opcode != EOpcode::FRAME_CON)
		{
			SDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::string UWebsocketClientSsl::encode_string_payload(const std::string& payload)
{
	std::string string_buffer;
	uint64 payload_length = payload.size();

	// FIN, RSV, Opcode
	uint8 byte1 = SDataFrame.fin ? 0x80 : 0x00;
	byte1 |= SDataFrame.rsv1 ? (uint8)ERSV::RSV1 : 0x00;
	byte1 |= SDataFrame.rsv2 ? (uint8)ERSV::RSV2 : 0x00;
	byte1 |= SDataFrame.rsv3 ? (uint8)ERSV::RSV3 : 0x00;
	byte1 |= (uint8)SDataFrame.opcode & 0x0F;
	string_buffer.push_back(byte1);

	// Mask and payload size
	uint8 byte2 = SDataFrame.mask ? 0x80 : 0x00;
	if (payload_length <= 125)
	{
		byte2 |= payload_length;
		string_buffer.push_back(byte2);
	}
	else if (payload_length <= 65535)
	{
		byte2 |= 126;
		string_buffer.push_back(byte2);
		string_buffer.push_back((payload_length >> 8) & 0xFF);
		string_buffer.push_back(payload_length & 0xFF);
	}
	else
	{
		byte2 |= 127;
		string_buffer.push_back(byte2);
		for (int i = 7; i >= 0; --i)
		{
			string_buffer.push_back((payload_length >> (8 * i)) & 0xFF);
		}
	}

	std::array<uint8, 4> masking_key;
	if (SDataFrame.mask)
	{
		masking_key = mask_gen();
		for (uint8 key : masking_key)
		{
			string_buffer.push_back(static_cast<uint8_t>(key));
		}
	}

	// payload data and mask
	for (size_t i = 0; i < payload.size(); ++i)
	{
		if (SDataFrame.mask)
		{
			string_buffer.push_back(payload[i] ^ static_cast<uint8_t>(masking_key[i % 4]));
		}
		else
		{
			string_buffer.push_back(payload[i]);
		}
	}

	return string_buffer;
}

void UWebsocketClientSsl::package_buffer(const TArray<uint8>& buffer)
{
	std::vector<uint8> payload;
	if (!SplitBuffer || buffer.Num() + get_frame_encode_size(buffer.Num()) <= MaxSendBufferSize)
	{
		payload.assign(buffer.GetData(), buffer.GetData() + buffer.Num());
		SDataFrame.fin = true;
		encode_buffer_payload(payload);
		asio::async_write(TCP.ssl_socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWebsocketClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		return;
	}

	SDataFrame.fin = false;
	size_t buffer_offset = 0;
	const size_t max_size = MaxSendBufferSize - get_frame_encode_size(buffer.Num());
	while (buffer_offset < buffer.Num())
	{
		SDataFrame.fin = buffer_offset < buffer.Num();
		size_t package_size = std::min(max_size, buffer.Num() - buffer_offset);
		payload.assign(buffer.GetData() + buffer_offset, buffer.GetData() + buffer_offset + package_size);
		encode_buffer_payload(payload);
		asio::async_write(TCP.ssl_socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWebsocketClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
		if (SDataFrame.opcode != EOpcode::FRAME_CON)
		{
			SDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::vector<uint8> UWebsocketClientSsl::encode_buffer_payload(const std::vector<uint8>& payload)
{
	std::vector<uint8> buffer;
	uint64 payload_length = payload.size();

	// FIN, RSV, Opcode
	uint8 byte1 = SDataFrame.fin ? 0x80 : 0x00;
	byte1 |= SDataFrame.rsv1 ? (uint8)ERSV::RSV1 : 0x00;
	byte1 |= SDataFrame.rsv2 ? (uint8)ERSV::RSV2 : 0x00;
	byte1 |= SDataFrame.rsv3 ? (uint8)ERSV::RSV3 : 0x00;
	byte1 |= (uint8)SDataFrame.opcode & 0x0F;
	buffer.push_back(byte1);

	// Mask and payload size
	uint8 byte2 = SDataFrame.mask ? 0x80 : 0x00;
	if (payload_length <= 125)
	{
		byte2 |= payload_length;
		buffer.push_back(byte2);
	}
	else if (payload_length <= 65535)
	{
		byte2 |= 126;
		buffer.push_back(byte2);
		buffer.push_back((payload_length >> 8) & 0xFF);
		buffer.push_back(payload_length & 0xFF);
	}
	else
	{
		byte2 |= 127;
		buffer.push_back(byte2);
		for (int i = 7; i >= 0; --i)
		{
			buffer.push_back((payload_length >> (8 * i)) & 0xFF);
		}
	}

	std::array<uint8, 4> masking_key;
	if (SDataFrame.mask)
	{
		masking_key = mask_gen();
		for (uint8 key : masking_key)
		{
			buffer.push_back(key);
		}
	}

	// payload data and mask
	for (size_t i = 0; i < payload.size(); ++i)
	{
		if (SDataFrame.mask)
		{
			buffer.push_back(payload[i] ^ masking_key[i % 4]);
		}
		else
		{
			buffer.push_back(payload[i]);
		}
	}

	return buffer;
}

size_t UWebsocketClientSsl::get_frame_encode_size(const size_t buffer_size)
{
	size_t size = 2;
	if (buffer_size <= 125)
	{
		size += 0;
	}
	else if (buffer_size <= 65535)
	{
		size += 2;
	}
	else
	{
		size += 8;
	}

	if (SDataFrame.mask)
	{
		size += 4;
	}

	return size;
}

std::array<uint8, 4> UWebsocketClientSsl::mask_gen()
{
	std::array<uint8, 4> maskKey;
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 255);

	for (uint8& byte : maskKey)
	{
		byte = dis(gen);
	}

	return maskKey;
}

bool UWebsocketClientSsl::decode_payload(FWsMessage& data_frame)
{
	if (ResponseBuffer.size() < 2)
	{
		return false;
	}

	size_t size = ResponseBuffer.size();
	std::vector<uint8> encoded_buffer;
	encoded_buffer.resize(size);
	asio::buffer_copy(asio::buffer(encoded_buffer, encoded_buffer.size()), ResponseBuffer.data());
	size_t pos = 0;

	// FIN, RSV, Opcode
	uint8 byte1 = encoded_buffer[pos++];
	data_frame.DataFrame.fin = byte1 & 0x80;
	data_frame.DataFrame.rsv1 = byte1 & 0x80;
	data_frame.DataFrame.rsv2 = byte1 & 0x40;
	data_frame.DataFrame.rsv3 = (uint8_t)byte1 & 0x10;
	data_frame.DataFrame.opcode = (EOpcode)((uint8_t)byte1 & 0x0F);

	// Mask and payload length
	uint8 byte2 = encoded_buffer[pos++];
	data_frame.DataFrame.mask = byte2 & 0x80;
	uint64 payload_length = byte2 & 0x7F;
	if (payload_length == 126)
	{
		if (encoded_buffer.size() < pos + 2)
		{
			return false;
		}
		payload_length = static_cast<uint64>((encoded_buffer[pos] << 8) | encoded_buffer[pos + 1]);
		pos += 2;
	}
	else if (payload_length == 127)
	{
		if (encoded_buffer.size() < pos + 8)
		{
			return false;
		}
		payload_length = 0;
		for (int i = 0; i < 8; ++i)
		{
			payload_length = (payload_length << 8) | encoded_buffer[pos + i];
		}
		pos += 8;
	}
	data_frame.DataFrame.length = payload_length;
	// Masking key
	if (data_frame.DataFrame.mask)
	{
		if (encoded_buffer.size() < pos + 4)
		{
			return false;
		}
		for (int i = 0; i < 4; ++i)
		{
			data_frame.DataFrame.masking_key.Add(encoded_buffer[pos++]);
		}
	}
	// Payload data
	if (encoded_buffer.size() < pos + payload_length)
	{
		return false;
	}
	//data_frame.Payload.reserve(payload_length);
	for (size_t i = 0; i < payload_length; ++i)
	{
		uint8 byte = encoded_buffer[pos++];
		if (data_frame.DataFrame.mask)
		{
			byte ^= data_frame.DataFrame.masking_key[i % 4];
		}
		data_frame.Payload.Add(byte);
	}
	return true;
}

void UWebsocketClientSsl::run_context_thread()
{
	MutexIO.Lock();
	while (TCP.attemps_fail <= MaxAttemp && !ShouldStopContext)
	{
		if (TCP.attemps_fail > 0)
		{
			OnConnectionWillRetry.Broadcast(TCP.attemps_fail);
		}
		TCP.error_code.clear();
		TCP.context.restart();
		TCP.resolver.async_resolve(TCHAR_TO_UTF8(*Host), TCHAR_TO_UTF8(*Service),
		                           std::bind(&UWebsocketClientSsl::resolve, this, asio::placeholders::error,
		                                     asio::placeholders::results)
		);
		TCP.context.run();
		if (!TCP.error_code)
		{
			break;
		}
		TCP.attemps_fail++;
		std::this_thread::sleep_for(std::chrono::seconds(Timeout));
	}
	consume_response_buffer();
	TCP.attemps_fail = 0;
	MutexIO.Unlock();
}

void UWebsocketClientSsl::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.
	TCP.endpoints = endpoints;

	asio::async_connect(TCP.ssl_socket.lowest_layer(), TCP.endpoints,
	                    std::bind(&UWebsocketClientSsl::conn, this, asio::placeholders::error)
	);
}

void UWebsocketClientSsl::conn(const std::error_code& error)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// The connection was successful;
	TCP.ssl_socket.async_handshake(asio::ssl::stream_base::client,
	                               std::bind(&UWebsocketClientSsl::ssl_handshake, this,
	                                         asio::placeholders::error));
}

void UWebsocketClientSsl::ssl_handshake(const std::error_code& error)
{
	// The connection was successful;
	FString request;
	request = "GET /" + Handshake.path + " HTTP/" + Handshake.version + "\r\n";
	request += "Host: " + Host + "\r\n";
	request += "Upgrade: websocket\r\n";
	request += "Connection: Upgrade\r\n";
	request += "Sec-WebSocket-Key: " + Handshake.Sec_WebSocket_Key + "\r\n";
	request += "Origin: " + Handshake.origin + "\r\n";
	request += "Sec-WebSocket-Protocol: " + Handshake.Sec_WebSocket_Protocol + "\r\n";
	request += "Sec-WebSocket-Version: " + Handshake.Sec_Websocket_Version + "\r\n";
	request += "\r\n";
	std::ostream request_stream(&RequestBuffer);
	request_stream << TCHAR_TO_UTF8(*request);
	asio::async_write(TCP.ssl_socket, RequestBuffer,
	                  std::bind(&UWebsocketClientSsl::write_handshake, this, asio::placeholders::error,
	                            asio::placeholders::bytes_transferred));
}

void UWebsocketClientSsl::write_handshake(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	// Read the response status line. The response_ streambuf will
	// automatically grow to accommodate the entire line. The growth may be
	// limited by passing a maximum size to the streambuf constructor.
	asio::async_read_until(TCP.ssl_socket, ResponseBuffer, "\r\n",
	                       std::bind(&UWebsocketClientSsl::read_handshake, this, asio::placeholders::error,
	                                 bytes_sent, asio::placeholders::bytes_transferred));
}

void UWebsocketClientSsl::read_handshake(const std::error_code& error, const size_t bytes_sent,
                                         const size_t bytes_recvd)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	// Check that response is OK.
	std::istream response_stream(&ResponseBuffer);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		OnError.Broadcast(-1, "Invalid response.");
		Close();
		return;
	}
	if (status_code != 101)
	{
		OnError.Broadcast(status_code, "Invalid status code.");
		Close();
		return;
	}

	asio::async_read_until(TCP.ssl_socket, ResponseBuffer, "\r\n\r\n",
	                       std::bind(&UWebsocketClientSsl::consume_header_buffer, this, asio::placeholders::error));
}

void UWebsocketClientSsl::consume_header_buffer(const std::error_code& error)
{
	consume_response_buffer();
	asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UWebsocketClientSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred));

	// The connection was successful
	OnConnected.Broadcast();
}

void UWebsocketClientSsl::write(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	OnMessageSent.Broadcast(bytes_sent);
}

void UWebsocketClientSsl::read(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		TCP.error_code = error;
		ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
				   error.message().c_str());
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	FWsMessage rDataFrame;
	if (!decode_payload(rDataFrame))
	{
		ResponseBuffer.consume(ResponseBuffer.size());
		asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
		                 std::bind(&UWebsocketClientSsl::read, this, asio::placeholders::error,
		                           asio::placeholders::bytes_transferred)
		);
		return;
	}

	if (rDataFrame.DataFrame.opcode == EOpcode::PING)
	{
		TArray<uint8> pong_buffer = {'p', 'o', 'n', 'g', '\0'};
		post_buffer(EOpcode::PONG, pong_buffer);
	}
	else if (rDataFrame.DataFrame.opcode == EOpcode::PONG)
	{
		OnPongReceived.Broadcast();
	}
	else if (rDataFrame.DataFrame.opcode == EOpcode::CONNECTION_CLOSE)
	{
		OnCloseNotify.Broadcast();
	}
	else
	{
		OnMessageReceived.Broadcast(rDataFrame);
	}

	consume_response_buffer();
	asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UWebsocketClientSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
}
