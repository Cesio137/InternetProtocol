// Fill out your copyright notice in the Description page of Project Settings.

#include "Websockets/WebsocketClient.h"

bool UWebsocketClient::send(const FString& message)
{
	if (!pool && !isConnected() && message.IsEmpty())
	{
		return false;
	}

	asio::post(*pool, [this, message]() { package_string(message); });
	return true;
}

bool UWebsocketClient::sendRaw(const TArray<uint8> buffer)
{
	if (!pool && !isConnected() && buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(*pool, [this, buffer]() { package_buffer(buffer); });
	return true;
}

bool UWebsocketClient::sendPing()
{
	if (!pool && !isConnected())
	{
		return false;
	}

	TArray<uint8> ping_buffer;
	ping_buffer.Add('\0');
	asio::post(*pool, std::bind(&UWebsocketClient::post_buffer, this, EOpcode::PING, ping_buffer));
	return true;
}

bool UWebsocketClient::asyncRead()
{
	if (!isConnected())
	{
		return false;
	}

	asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(2),
	                 std::bind(&UWebsocketClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred));
	return true;
}

bool UWebsocketClient::connect()
{
	if (!pool && !isConnected())
	{
		return false;
	}

	asio::post(*pool, std::bind(&UWebsocketClient::run_context_thread, this));
	return true;
}

void UWebsocketClient::close()
{
	tcp.context.stop();
	tcp.socket.shutdown(asio::ip::tcp::socket::shutdown_both, tcp.error_code);
	if (tcp.error_code)
		OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
	tcp.socket.close(tcp.error_code);
	if (tcp.error_code)
		OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
	OnClose.Broadcast();
}

void UWebsocketClient::post_string(const FString& str)
{
	mutexBuffer.lock();
	sDataFrame.opcode = EOpcode::TEXT_FRAME;
	package_string(str);
	mutexBuffer.unlock();
}

void UWebsocketClient::post_buffer(EOpcode opcode, const TArray<uint8>& buffer)
{
	mutexBuffer.lock();
	sDataFrame.opcode = opcode;
	if (opcode == EOpcode::BINARY_FRAME)
	{
		package_buffer(buffer);
	}
	else if (opcode == EOpcode::PING || opcode == EOpcode::PONG)
	{
		std::vector<uint8> p_buffer(buffer.GetData(), buffer.GetData() + buffer.Num());
		encode_buffer_payload(p_buffer);
		asio::async_write(tcp.socket, asio::buffer(p_buffer.data(), p_buffer.size()),
		                  std::bind(&UWebsocketClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred));
	}
	mutexBuffer.unlock();
}

void UWebsocketClient::package_string(const FString& str)
{
	std::string payload;
	if (!splitBuffer || str.Len() /*+ get_frame_encode_size(str.size())*/ <= maxSendBufferSize)
	{
		sDataFrame.fin = true;
		payload = TCHAR_TO_UTF8(*str);
		payload = encode_string_payload(payload);
		asio::async_write(tcp.socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWebsocketClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		return;
	}

	sDataFrame.fin = true;
	size_t string_offset = 0;
	const size_t max_size = maxSendBufferSize - get_frame_encode_size(str.Len());
	while (string_offset < str.Len())
	{
		sDataFrame.fin = string_offset < str.Len();
		size_t package_size = std::min(max_size, str.Len() - string_offset);
		FString strshrink = str.Mid(string_offset, package_size);
		payload = TCHAR_TO_UTF8(*strshrink);
		payload = encode_string_payload(payload);
		asio::async_write(tcp.socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWebsocketClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		string_offset += package_size;
		if (sDataFrame.opcode != EOpcode::FRAME_CON)
		{
			sDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::string UWebsocketClient::encode_string_payload(const std::string& payload)
{
	std::string string_buffer;
	uint64 payload_length = payload.size();

	// FIN, RSV, Opcode
	uint8 byte1 = sDataFrame.fin ? 0x80 : 0x00;
	byte1 |= sDataFrame.rsv1 ? (uint8)ERSV::RSV1 : 0x00;
	byte1 |= sDataFrame.rsv2 ? (uint8)ERSV::RSV2 : 0x00;
	byte1 |= sDataFrame.rsv3 ? (uint8)ERSV::RSV3 : 0x00;
	byte1 |= (uint8)sDataFrame.opcode & 0x0F;
	string_buffer.push_back(byte1);

	// Mask and payload size
	uint8 byte2 = sDataFrame.mask ? 0x80 : 0x00;
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
	if (sDataFrame.mask)
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
		if (sDataFrame.mask)
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
	if (!splitBuffer || buffer.Num() + get_frame_encode_size(buffer.Num()) <= maxSendBufferSize)
	{
		payload.assign(buffer.GetData(), buffer.GetData() + buffer.Num());
		sDataFrame.fin = true;
		encode_buffer_payload(payload);
		asio::async_write(tcp.socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWebsocketClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		return;
	}

	sDataFrame.fin = false;
	size_t buffer_offset = 0;
	const size_t max_size = maxSendBufferSize - get_frame_encode_size(buffer.Num());
	while (buffer_offset < buffer.Num())
	{
		sDataFrame.fin = buffer_offset < buffer.Num();
		size_t package_size = std::min(max_size, buffer.Num() - buffer_offset);
		payload.assign(buffer.GetData() + buffer_offset, buffer.GetData() + buffer_offset + package_size);
		encode_buffer_payload(payload);
		asio::async_write(tcp.socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWebsocketClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
		if (sDataFrame.opcode != EOpcode::FRAME_CON)
		{
			sDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::vector<uint8> UWebsocketClient::encode_buffer_payload(const std::vector<uint8>& payload)
{
	std::vector<uint8> buffer;
	uint64 payload_length = payload.size();

	// FIN, RSV, Opcode
	uint8 byte1 = sDataFrame.fin ? 0x80 : 0x00;
	byte1 |= sDataFrame.rsv1 ? (uint8)ERSV::RSV1 : 0x00;
	byte1 |= sDataFrame.rsv2 ? (uint8)ERSV::RSV2 : 0x00;
	byte1 |= sDataFrame.rsv3 ? (uint8)ERSV::RSV3 : 0x00;
	byte1 |= (uint8)sDataFrame.opcode & 0x0F;
	buffer.push_back(byte1);

	// Mask and payload size
	uint8 byte2 = sDataFrame.mask ? 0x80 : 0x00;
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
	if (sDataFrame.mask)
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
		if (sDataFrame.mask)
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

	if (sDataFrame.mask)
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
	if (response_buffer.size() < 2)
	{
		return false;
	}

	size_t size = response_buffer.size();
	std::vector<uint8> encoded_buffer;
	encoded_buffer.resize(size);
	asio::buffer_copy(asio::buffer(encoded_buffer, encoded_buffer.size()), response_buffer.data());
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
	mutexIO.lock();
	while (tcp.attemps_fail <= maxAttemp)
	{
		if (tcp.attemps_fail > 0)
		{
			OnConnectionWillRetry.Broadcast(tcp.attemps_fail);
		}
		tcp.error_code.clear();
		tcp.context.restart();
		tcp.resolver.async_resolve(TCHAR_TO_UTF8(*host), TCHAR_TO_UTF8(*service),
		                           std::bind(&UWebsocketClient::resolve, this, asio::placeholders::error,
		                                     asio::placeholders::endpoint));
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

void UWebsocketClient::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		tcp.error_code = error;
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.
	tcp.endpoints = endpoints;

	asio::async_connect(tcp.socket, tcp.endpoints,
	                    std::bind(&UWebsocketClient::conn, this, asio::placeholders::error)
	);
}

void UWebsocketClient::conn(const std::error_code& error)
{
	if (error)
	{
		tcp.error_code = error;
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	// The connection was successful;
	FString request;
	request = "GET /" + handshake.path + " HTTP/" + handshake.version + "\r\n";
	request += "Host: " + host + "\r\n";
	request += "Upgrade: websocket\r\n";
	request += "Connection: Upgrade\r\n";
	request += "Sec-WebSocket-Key: " + handshake.Sec_WebSocket_Key + "\r\n";
	request += "Origin: " + handshake.origin + "\r\n";
	request += "Sec-WebSocket-Protocol: " + handshake.Sec_WebSocket_Protocol + "\r\n";
	request += "Sec-WebSocket-Version: " + handshake.Sec_Websocket_Version + "\r\n";
	request += "\r\n";
	std::ostream request_stream(&request_buffer);
	request_stream << TCHAR_TO_UTF8(*request);
	asio::async_write(tcp.socket, request_buffer,
	                  std::bind(&UWebsocketClient::write_handshake, this, asio::placeholders::error,
	                            asio::placeholders::bytes_transferred));
}

void UWebsocketClient::write_handshake(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		tcp.error_code = error;
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	// Read the response status line. The response_ streambuf will
	// automatically grow to accommodate the entire line. The growth may be
	// limited by passing a maximum size to the streambuf constructor.
	asio::async_read_until(tcp.socket, response_buffer, "\r\n",
	                       std::bind(&UWebsocketClient::read_handshake, this, asio::placeholders::error,
	                                 bytes_sent, asio::placeholders::bytes_transferred));
}

void UWebsocketClient::read_handshake(const std::error_code& error, const size_t bytes_sent,
                                      const size_t bytes_recvd)
{
	if (error)
	{
		tcp.error_code = error;
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	// Check that response is OK.
	std::istream response_stream(&response_buffer);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		OnError.Broadcast(-1, "Invalid response.");
		close();
		return;
	}
	if (status_code != 101)
	{
		OnError.Broadcast(status_code, "Invalid status code.");
		close();
		return;
	}

	asio::async_read_until(tcp.socket, response_buffer, "\r\n\r\n",
	                       std::bind(&UWebsocketClient::consume_header_buffer, this, asio::placeholders::error));
}

void UWebsocketClient::consume_header_buffer(const std::error_code& error)
{
	consume_response_buffer();
	asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
	                 std::bind(&UWebsocketClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred));

	// The connection was successful
	OnConnected.Broadcast();
}

void UWebsocketClient::write(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		tcp.error_code = error;
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	OnMessageSent.Broadcast(bytes_sent);
}

void UWebsocketClient::read(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		tcp.error_code = error;
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	FWsMessage rDataFrame;
	if (!decode_payload(rDataFrame))
	{
		response_buffer.consume(response_buffer.size());
		asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
		                 std::bind(&UWebsocketClient::read, this, asio::placeholders::error,
		                           asio::placeholders::bytes_transferred)
		);
		return;
	}

	if (rDataFrame.DataFrame.opcode == EOpcode::PING)
	{
		TArray<uint8> pong_buffer;
		pong_buffer.Add('\n');
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
	asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
	                 std::bind(&UWebsocketClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
}

bool UWebsocketClientSsl::send(const FString& message)
{
	if (!pool && !isConnected() && message.IsEmpty())
	{
		return false;
	}

	asio::post(*pool, [this, message]() { package_string(message); });
	return true;
}

bool UWebsocketClientSsl::sendRaw(const TArray<uint8> buffer)
{
	if (!pool && !isConnected() && buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(*pool, [this, buffer]() { package_buffer(buffer); });
	return true;
}

bool UWebsocketClientSsl::sendPing()
{
	if (!pool && !isConnected())
	{
		return false;
	}

	TArray<uint8> ping_buffer;
	ping_buffer.Add('\0');
	asio::post(*pool, std::bind(&UWebsocketClientSsl::post_buffer, this, EOpcode::PING, ping_buffer));
	return true;
}

bool UWebsocketClientSsl::asyncRead()
{
	if (!isConnected())
	{
		return false;
	}

	asio::async_read(tcp.ssl_socket, response_buffer, asio::transfer_at_least(2),
	                 std::bind(&UWebsocketClientSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred));
	return true;
}

bool UWebsocketClientSsl::connect()
{
	if (!pool && !isConnected())
	{
		return false;
	}

	asio::post(*pool, std::bind(&UWebsocketClientSsl::run_context_thread, this));
	return true;
}

void UWebsocketClientSsl::close()
{
	tcp.context.stop();
	tcp.ssl_socket.shutdown(tcp.error_code);
	tcp.ssl_socket.async_shutdown([&](const std::error_code& error)
	{
		if (error)
		{
			tcp.error_code = error;
			OnError.Broadcast(error.value(), error.message().c_str());
			tcp.error_code.clear();
		}
		tcp.ssl_socket.lowest_layer().shutdown(
			asio::ip::tcp::socket::shutdown_both, tcp.error_code);
		if (tcp.error_code)
			OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
		tcp.ssl_socket.lowest_layer().close(tcp.error_code);
		if (tcp.error_code)
			OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
		OnClose.Broadcast();
	});
}

void UWebsocketClientSsl::post_string(const FString& str)
{
	mutexBuffer.lock();
	sDataFrame.opcode = EOpcode::TEXT_FRAME;
	package_string(str);
	mutexBuffer.unlock();
}

void UWebsocketClientSsl::post_buffer(EOpcode opcode, const TArray<uint8>& buffer)
{
	mutexBuffer.lock();
	sDataFrame.opcode = opcode;
	if (opcode == EOpcode::BINARY_FRAME)
	{
		package_buffer(buffer);
	}
	else if (opcode == EOpcode::PING || opcode == EOpcode::PONG)
	{
		std::vector<uint8> p_buffer(buffer.GetData(), buffer.GetData() + buffer.Num());
		encode_buffer_payload(p_buffer);
		asio::async_write(tcp.ssl_socket, asio::buffer(p_buffer.data(), p_buffer.size()),
		                  std::bind(&UWebsocketClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred));
	}
	mutexBuffer.unlock();
}

void UWebsocketClientSsl::package_string(const FString& str)
{
	std::string payload;
	if (!splitBuffer || str.Len() /*+ get_frame_encode_size(str.size())*/ <= maxSendBufferSize)
	{
		sDataFrame.fin = true;
		payload = TCHAR_TO_UTF8(*str);
		payload = encode_string_payload(payload);
		asio::async_write(tcp.ssl_socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWebsocketClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		return;
	}

	sDataFrame.fin = true;
	size_t string_offset = 0;
	const size_t max_size = maxSendBufferSize - get_frame_encode_size(str.Len());
	while (string_offset < str.Len())
	{
		sDataFrame.fin = string_offset < str.Len();
		size_t package_size = std::min(max_size, str.Len() - string_offset);
		FString strshrink = str.Mid(string_offset, package_size);
		payload = TCHAR_TO_UTF8(*strshrink);
		payload = encode_string_payload(payload);
		asio::async_write(tcp.ssl_socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWebsocketClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		string_offset += package_size;
		if (sDataFrame.opcode != EOpcode::FRAME_CON)
		{
			sDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::string UWebsocketClientSsl::encode_string_payload(const std::string& payload)
{
	std::string string_buffer;
	uint64 payload_length = payload.size();

	// FIN, RSV, Opcode
	uint8 byte1 = sDataFrame.fin ? 0x80 : 0x00;
	byte1 |= sDataFrame.rsv1 ? (uint8)ERSV::RSV1 : 0x00;
	byte1 |= sDataFrame.rsv2 ? (uint8)ERSV::RSV2 : 0x00;
	byte1 |= sDataFrame.rsv3 ? (uint8)ERSV::RSV3 : 0x00;
	byte1 |= (uint8)sDataFrame.opcode & 0x0F;
	string_buffer.push_back(byte1);

	// Mask and payload size
	uint8 byte2 = sDataFrame.mask ? 0x80 : 0x00;
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
	if (sDataFrame.mask)
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
		if (sDataFrame.mask)
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
	if (!splitBuffer || buffer.Num() + get_frame_encode_size(buffer.Num()) <= maxSendBufferSize)
	{
		payload.assign(buffer.GetData(), buffer.GetData() + buffer.Num());
		sDataFrame.fin = true;
		encode_buffer_payload(payload);
		asio::async_write(tcp.ssl_socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWebsocketClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		return;
	}

	sDataFrame.fin = false;
	size_t buffer_offset = 0;
	const size_t max_size = maxSendBufferSize - get_frame_encode_size(buffer.Num());
	while (buffer_offset < buffer.Num())
	{
		sDataFrame.fin = buffer_offset < buffer.Num();
		size_t package_size = std::min(max_size, buffer.Num() - buffer_offset);
		payload.assign(buffer.GetData() + buffer_offset, buffer.GetData() + buffer_offset + package_size);
		encode_buffer_payload(payload);
		asio::async_write(tcp.ssl_socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWebsocketClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
		if (sDataFrame.opcode != EOpcode::FRAME_CON)
		{
			sDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::vector<uint8> UWebsocketClientSsl::encode_buffer_payload(const std::vector<uint8>& payload)
{
	std::vector<uint8> buffer;
	uint64 payload_length = payload.size();

	// FIN, RSV, Opcode
	uint8 byte1 = sDataFrame.fin ? 0x80 : 0x00;
	byte1 |= sDataFrame.rsv1 ? (uint8)ERSV::RSV1 : 0x00;
	byte1 |= sDataFrame.rsv2 ? (uint8)ERSV::RSV2 : 0x00;
	byte1 |= sDataFrame.rsv3 ? (uint8)ERSV::RSV3 : 0x00;
	byte1 |= (uint8)sDataFrame.opcode & 0x0F;
	buffer.push_back(byte1);

	// Mask and payload size
	uint8 byte2 = sDataFrame.mask ? 0x80 : 0x00;
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
	if (sDataFrame.mask)
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
		if (sDataFrame.mask)
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

	if (sDataFrame.mask)
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
	if (response_buffer.size() < 2)
	{
		return false;
	}

	size_t size = response_buffer.size();
	std::vector<uint8> encoded_buffer;
	encoded_buffer.resize(size);
	asio::buffer_copy(asio::buffer(encoded_buffer, encoded_buffer.size()), response_buffer.data());
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
	mutexIO.lock();
	while (tcp.attemps_fail <= maxAttemp)
	{
		if (tcp.attemps_fail > 0)
		{
			OnConnectionWillRetry.Broadcast(tcp.attemps_fail);
		}
		tcp.error_code.clear();
		tcp.context.restart();
		tcp.resolver.async_resolve(TCHAR_TO_UTF8(*host), TCHAR_TO_UTF8(*service),
		                           std::bind(&UWebsocketClientSsl::resolve, this, asio::placeholders::error,
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

void UWebsocketClientSsl::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		tcp.error_code = error;
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.
	tcp.endpoints = endpoints;

	asio::async_connect(tcp.ssl_socket.lowest_layer(), tcp.endpoints,
	                    std::bind(&UWebsocketClientSsl::conn, this, asio::placeholders::error)
	);
}

void UWebsocketClientSsl::conn(const std::error_code& error)
{
	if (error)
	{
		tcp.error_code = error;
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}
	// The connection was successful;
	tcp.ssl_socket.async_handshake(asio::ssl::stream_base::client,
	                               std::bind(&UWebsocketClientSsl::ssl_handshake, this,
	                                         asio::placeholders::error));
}

void UWebsocketClientSsl::ssl_handshake(const std::error_code& error)
{
	// The connection was successful;
	FString request;
	request = "GET /" + handshake.path + " HTTP/" + handshake.version + "\r\n";
	request += "Host: " + host + "\r\n";
	request += "Upgrade: websocket\r\n";
	request += "Connection: Upgrade\r\n";
	request += "Sec-WebSocket-Key: " + handshake.Sec_WebSocket_Key + "\r\n";
	request += "Origin: " + handshake.origin + "\r\n";
	request += "Sec-WebSocket-Protocol: " + handshake.Sec_WebSocket_Protocol + "\r\n";
	request += "Sec-WebSocket-Version: " + handshake.Sec_Websocket_Version + "\r\n";
	request += "\r\n";
	std::ostream request_stream(&request_buffer);
	request_stream << TCHAR_TO_UTF8(*request);
	asio::async_write(tcp.ssl_socket, request_buffer,
	                  std::bind(&UWebsocketClientSsl::write_handshake, this, asio::placeholders::error,
	                            asio::placeholders::bytes_transferred));
}

void UWebsocketClientSsl::write_handshake(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		tcp.error_code = error;
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	// Read the response status line. The response_ streambuf will
	// automatically grow to accommodate the entire line. The growth may be
	// limited by passing a maximum size to the streambuf constructor.
	asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n",
	                       std::bind(&UWebsocketClientSsl::read_handshake, this, asio::placeholders::error,
	                                 bytes_sent, asio::placeholders::bytes_transferred));
}

void UWebsocketClientSsl::read_handshake(const std::error_code& error, const size_t bytes_sent,
                                         const size_t bytes_recvd)
{
	if (error)
	{
		tcp.error_code = error;
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	// Check that response is OK.
	std::istream response_stream(&response_buffer);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		OnError.Broadcast(-1, "Invalid response.");
		close();
		return;
	}
	if (status_code != 101)
	{
		OnError.Broadcast(status_code, "Invalid status code.");
		close();
		return;
	}

	asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n\r\n",
	                       std::bind(&UWebsocketClientSsl::consume_header_buffer, this, asio::placeholders::error));
}

void UWebsocketClientSsl::consume_header_buffer(const std::error_code& error)
{
	consume_response_buffer();
	asio::async_read(tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
	                 std::bind(&UWebsocketClientSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred));

	// The connection was successful
	OnConnected.Broadcast();
}

void UWebsocketClientSsl::write(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		tcp.error_code = error;
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	OnMessageSent.Broadcast(bytes_sent);
}

void UWebsocketClientSsl::read(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		tcp.error_code = error;
		OnError.Broadcast(error.value(), error.message().c_str());
		return;
	}

	FWsMessage rDataFrame;
	if (!decode_payload(rDataFrame))
	{
		response_buffer.consume(response_buffer.size());
		asio::async_read(tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
		                 std::bind(&UWebsocketClientSsl::read, this, asio::placeholders::error,
		                           asio::placeholders::bytes_transferred)
		);
		return;
	}

	if (rDataFrame.DataFrame.opcode == EOpcode::PING)
	{
		TArray<uint8> pong_buffer;
		pong_buffer.Add('\n');
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
	asio::async_read(tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
	                 std::bind(&UWebsocketClientSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
}
