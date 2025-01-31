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

#include "WS/WSServer.h"

bool UWSServer::SendHandshakeTo(const FServerRequest& Request, FServerResponse Response, const FTCPSocket& Socket)
{
	if (!Socket.SmartPtr->is_open())
	{
		return false;
	}
	asio::post(GetThreadPool(),
	           std::bind(&UWSServer::package_handshake, this, Request, Response, Socket.SmartPtr, 101));
	return true;
}

bool UWSServer::SendHandshakeErrorTo(const int StatusCode, const FString& Body, const FTCPSocket& Socket)
{
	if (!Socket.SmartPtr->is_open())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UWSServer::package_handshake_error, this, StatusCode, Body, Socket.SmartPtr));
	return true;
}

bool UWSServer::SendStrTo(const FString& Message, const FTCPSocket& Socket)
{
	if (!Socket.SmartPtr->is_open() || Message.IsEmpty())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UWSServer::package_string, this, Message, Socket.SmartPtr));
	return true;
}

bool UWSServer::SendBufferTo(const TArray<uint8> Buffer, const FTCPSocket& Socket)
{
	if (!Socket.SmartPtr->is_open() || Buffer.Num() == 0)
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UWSServer::package_buffer, this, Buffer, Socket.SmartPtr));
	return true;
}

bool UWSServer::SendPingTo(const FTCPSocket& Socket)
{
	if (!Socket.SmartPtr->is_open())
	{
		return false;
	}

	TArray<uint8> ping_buffer = {'p', 'i', 'n', 'g', '\0'};
	asio::post(GetThreadPool(), std::bind(&UWSServer::post_buffer, this,
	                                      EOpcode::PING, ping_buffer, Socket.SmartPtr));
	return true;
}

bool UWSServer::Open()
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

	asio::post(GetThreadPool(), std::bind(&UWSServer::run_context_thread, this));
	return true;
}

void UWSServer::Close()
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
	if (ListenerBuffers.Num() > 0)
	{
		ListenerBuffers.Empty();
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

void UWSServer::DisconnectSocket(const FTCPSocket& Socket)
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
	if (ListenerBuffers.Contains(Socket.SmartPtr))
	{
		ListenerBuffers.Remove(Socket.SmartPtr);
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

void UWSServer::disconnect_socket_after_error(const asio::error_code& error, const socket_ptr& socket)
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
	if (ListenerBuffers.Contains(socket))
	{
		ListenerBuffers.Remove(socket);
	}
	if (TCP.sockets.Contains(socket))
	{
		TCP.sockets.Remove(socket);
	}
	OnSocketDisconnected.Broadcast(error, socket);
}

void UWSServer::post_string(const FString& str, const socket_ptr& socket)
{
	FScopeLock Guard(&MutexBuffer);
	SDataFrame.opcode = EOpcode::TEXT_FRAME;
	package_string(str, socket);
}

void UWSServer::post_buffer(EOpcode opcode, const TArray<uint8>& buffer, const socket_ptr& socket)
{
	FScopeLock Guard(&MutexBuffer);
	SDataFrame.opcode = opcode;
	if (opcode == EOpcode::BINARY_FRAME)
	{
		package_buffer(buffer, socket);
	}
	else if (opcode == EOpcode::PING || opcode == EOpcode::PONG)
	{
		std::vector<uint8> p_buffer(buffer.GetData(), buffer.GetData() + buffer.Num());
		encode_buffer_payload(p_buffer);
		asio::async_write(*socket, asio::buffer(p_buffer.data(), p_buffer.size()),
		                  std::bind(&UWSServer::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, socket));
	}
}

void UWSServer::package_string(const FString& str, const socket_ptr& socket)
{
	std::string payload;
	const int frame_encode_size = static_cast<int>(get_frame_encode_size(str.Len()));
	if (!SplitBuffer || str.Len() + frame_encode_size <= MaxSendBufferSize)
	{
		SDataFrame.fin = true;
		payload = TCHAR_TO_UTF8(*str);
		payload = encode_string_payload(payload);
		asio::async_write(*socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWSServer::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, socket));
		return;
	}

	SDataFrame.fin = true;
	size_t string_offset = 0;
	const size_t max_size = MaxSendBufferSize - get_frame_encode_size(str.Len());
	const size_t str_len = static_cast<size_t>(str.Len());
	while (string_offset < str_len)
	{
		SDataFrame.fin = string_offset < str_len;
		size_t package_size = std::min(max_size, str_len - string_offset);
		FString strshrink = str.Mid(string_offset, package_size);
		payload = TCHAR_TO_UTF8(*strshrink);
		payload = encode_string_payload(payload);
		asio::async_write(*socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWSServer::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, socket));
		string_offset += package_size;
		if (SDataFrame.opcode != EOpcode::FRAME_CON)
		{
			SDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::string UWSServer::encode_string_payload(const std::string& payload)
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

void UWSServer::package_buffer(const TArray<uint8>& buffer, const socket_ptr& socket)
{
	std::vector<uint8> payload;
	const int frame_encode_size = static_cast<int>(get_frame_encode_size(buffer.Num()));
	if (!SplitBuffer || buffer.Num() + frame_encode_size <= MaxSendBufferSize)
	{
		payload.assign(buffer.GetData(), buffer.GetData() + buffer.Num());
		SDataFrame.fin = true;
		encode_buffer_payload(payload);
		asio::async_write(*socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWSServer::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, socket));
		return;
	}

	SDataFrame.fin = false;
	size_t buffer_offset = 0;
	const size_t max_size = MaxSendBufferSize - get_frame_encode_size(buffer.Num());
	const size_t buf_len = static_cast<size_t>(buffer.Num());
	while (buffer_offset < buf_len)
	{
		SDataFrame.fin = buffer_offset < buf_len;
		size_t package_size = std::min(max_size, buf_len - buffer_offset);
		payload.assign(buffer.GetData() + buffer_offset, buffer.GetData() + buffer_offset + package_size);
		encode_buffer_payload(payload);
		asio::async_write(*socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWSServer::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, socket));
		buffer_offset += package_size;
		if (SDataFrame.opcode != EOpcode::FRAME_CON)
		{
			SDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::vector<uint8> UWSServer::encode_buffer_payload(const std::vector<uint8>& payload)
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

size_t UWSServer::get_frame_encode_size(const size_t buffer_size)
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

std::array<uint8, 4> UWSServer::mask_gen()
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

bool UWSServer::decode_payload(FWsMessage& data_frame, const socket_ptr& socket)
{
	if (ListenerBuffers[socket]->size() < 2)
	{
		return false;
	}

	size_t size = ListenerBuffers[socket]->size();
	std::vector<uint8> encoded_buffer;
	encoded_buffer.resize(size);
	asio::buffer_copy(asio::buffer(encoded_buffer, encoded_buffer.size()), ListenerBuffers[socket]->data());
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

TArray<uint8> UWSServer::sha1(const FString& input)
{
	const uint32_t h0 = 0x67452301;
	const uint32_t h1 = 0xEFCDAB89;
	const uint32_t h2 = 0x98BADCFE;
	const uint32_t h3 = 0x10325476;
	const uint32_t h4 = 0xC3D2E1F0;

	uint64_t bit_length = input.Len() * 8;
	std::string padded_input = TCHAR_TO_UTF8(*input);

	padded_input += static_cast<char>(0x80);

	while ((padded_input.size() % 64) != 56)
	{
		padded_input += static_cast<char>(0x00);
	}

	for (int i = 7; i >= 0; --i)
	{
		padded_input += static_cast<char>((bit_length >> (i * 8)) & 0xFF);
	}

	uint32_t h[5] = {h0, h1, h2, h3, h4};

	for (size_t i = 0; i < padded_input.size(); i += 64)
	{
		uint32_t w[80] = {0};

		for (int j = 0; j < 16; ++j)
		{
			w[j] = static_cast<uint8_t>(padded_input[i + j * 4]) << 24 |
				static_cast<uint8_t>(padded_input[i + j * 4 + 1]) << 16 |
				static_cast<uint8_t>(padded_input[i + j * 4 + 2]) << 8 |
				static_cast<uint8_t>(padded_input[i + j * 4 + 3]);
		}

		for (int j = 16; j < 80; ++j)
		{
			w[j] = (w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16]);
			w[j] = (w[j] << 1) | (w[j] >> 31);
		}

		uint32_t a = h[0];
		uint32_t b = h[1];
		uint32_t c = h[2];
		uint32_t d = h[3];
		uint32_t e = h[4];

		for (int j = 0; j < 80; ++j)
		{
			uint32_t f, k;

			if (j < 20)
			{
				f = (b & c) | ((~b) & d);
				k = 0x5A827999;
			}
			else if (j < 40)
			{
				f = b ^ c ^ d;
				k = 0x6ED9EBA1;
			}
			else if (j < 60)
			{
				f = (b & c) | (b & d) | (c & d);
				k = 0x8F1BBCDC;
			}
			else
			{
				f = b ^ c ^ d;
				k = 0xCA62C1D6;
			}

			uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[j];
			e = d;
			d = c;
			c = (b << 30) | (b >> 2);
			b = a;
			a = temp;
		}

		h[0] += a;
		h[1] += b;
		h[2] += c;
		h[3] += d;
		h[4] += e;
	}

	TArray<uint8> hash;
	hash.SetNum(20);
	for (int i = 0; i < 5; ++i)
	{
		hash[i * 4] = (h[i] >> 24) & 0xFF;
		hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
		hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
		hash[i * 4 + 3] = h[i] & 0xFF;
	}

	return hash;
}

FString UWSServer::base64_encode(const uint8_t* input, size_t length)
{
	const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::string result;
	int i = 0;
	uint32_t octet_a, octet_b, octet_c, triple;

	while (length >= 3)
	{
		octet_a = input[i++];
		octet_b = input[i++];
		octet_c = input[i++];

		triple = (octet_a << 16) + (octet_b << 8) + octet_c;

		result.push_back(base64_chars[(triple >> 18) & 0x3F]);
		result.push_back(base64_chars[(triple >> 12) & 0x3F]);
		result.push_back(base64_chars[(triple >> 6) & 0x3F]);
		result.push_back(base64_chars[triple & 0x3F]);

		length -= 3;
	}

	if (length > 0)
	{
		octet_a = input[i++];
		octet_b = length > 1 ? input[i++] : 0;

		triple = (octet_a << 16) + (octet_b << 8);

		result.push_back(base64_chars[(triple >> 18) & 0x3F]);
		result.push_back(base64_chars[(triple >> 12) & 0x3F]);
		result.push_back(length == 2 ? base64_chars[(triple >> 6) & 0x3F] : '=');
		result.push_back('=');
	}

	return UTF8_TO_TCHAR(result.c_str());
}

FString UWSServer::generate_accept_key(const FString& sec_websocket_key)
{
	const FString magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	FString concatenated = sec_websocket_key + magic_string;
	auto hash = sha1(concatenated);
	return base64_encode(hash.GetData(), hash.Num());
}

void UWSServer::package_handshake(const FServerRequest& req, FServerResponse& res, const socket_ptr& socket,
                                  const uint32_t status_code)
{
	if (req.Headers.Contains("Sec-WebSocket-Key"))
	{
		FString key = req.Headers["Sec-WebSocket-Key"];
		FString accept_key = generate_accept_key(key);
		res.Headers.Add("Sec-WebSocket-Accept", accept_key);
	}
	if (req.Headers.Contains("Sec-WebSocket-Protocol"))
	{
		FString protocol = req.Headers["Sec-WebSocket-Protocol"];
		if (protocol.Find("chat") != INDEX_NONE || protocol.Find("superchat") != INDEX_NONE)
		{
			res.Headers.Add("Sec-WebSocket-Protocol", "chat");
		}
		else if (protocol.Find("json") != INDEX_NONE)
		{
			res.Headers.Add("Sec-WebSocket-Protocol", "json");
		}
		else if (protocol.Find("xml") != INDEX_NONE)
		{
			res.Headers.Add("Sec-WebSocket-Protocol", "xml");
		}
	}
	FString payload = "HTTP/" + res.Version + " 101 Switching Protocols\r\n";
	if (!res.Headers.Num() == 0)
	{
		for (const TPair<FString, FString> header : res.Headers)
		{
			payload += header.Key + ": " + header.Value + "\r\n";
		}
	}
	payload += "\r\n";
	std::string utf8payload = TCHAR_TO_UTF8(*payload);
	asio::async_write(
		*socket, asio::buffer(utf8payload.data(), utf8payload.size()),
		std::bind(&UWSServer::write_handshake, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred, socket, status_code));
}

void UWSServer::package_handshake_error(const int status_code, const FString& body, const socket_ptr& socket)
{
	std::string payload;
	if (ResponseStatusCode.Contains(status_code))
	{
		payload = "HTTP/1.1 " + std::to_string(status_code) + " " + TCHAR_TO_UTF8(*ResponseStatusCode[status_code]) +
			"\r\n";
	}
	else
	{
		payload = "HTTP/1.1 400 HTTP Bad Request\r\n";
	}
	std::string body_content = body.IsEmpty() ? TCHAR_TO_UTF8(*body) : "";
	std::string len = std::to_string(body_content.length());
	switch (status_code)
	{
	case 400:
		payload += "Content-Type: text/plain\r\n"
		"Content-Length: " + len + "\r\n"
		"Connection: close\r\n"
		"\r\n";
		payload += body_content;
		payload += "\r\n";
		break;
	case 405:
		payload += "Allow: GET\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n\r\n";
		break;
	case 505:
		payload += "Content-Type: text/plain\r\n"
		"Content-Length: 27\r\n"
		"Connection: close\r\n"
		"\r\n"
		"HTTP version not supported.\r\n";
		break;
	default: payload += "\r\n";
	}
	asio::async_write(
		*socket, asio::buffer(payload.data(), payload.size()),
		std::bind(&UWSServer::write_handshake, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred, socket, status_code));
}

void UWSServer::consume_listening_buffer(const socket_ptr& socket)
{
	const size_t res_size = ListenerBuffers[socket]->size();
	if (res_size > 0)
	{
		ListenerBuffers[socket]->consume(res_size);
	}
}

void UWSServer::run_context_thread()
{
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	socket_ptr conn_socket = MakeShared<asio::ip::tcp::socket>(TCP.context);
	TCP.acceptor.async_accept(
		*conn_socket, std::bind(&UWSServer::accept, this, asio::placeholders::error, conn_socket));
	TCP.context.run();
	if (!IsClosing)
	{
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			Close();
		});
	}
}

void UWSServer::accept(const asio::error_code& error, socket_ptr& socket)
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
				disconnect_socket_after_error(error, socket);
		});
		if (TCP.acceptor.is_open())
		{
			socket_ptr conn_socket = MakeShared<asio::ip::tcp::socket>(TCP.context);
			TCP.acceptor.async_accept(
				*conn_socket, std::bind(&UWSServer::accept, this, asio::placeholders::error, conn_socket));
		}
		return;
	}

	if (TCP.sockets.Num() < Backlog)
	{
		TCP.sockets.Add(socket);
		TSharedPtr<asio::streambuf> listening_buffer = MakeShared<asio::streambuf>();
		ListenerBuffers.Add(socket, listening_buffer);
		asio::async_read_until(*socket, *listening_buffer, "\r\n",
							   std::bind(&UWSServer::read_handshake, this,
										 asio::placeholders::error,
										 asio::placeholders::bytes_transferred, socket));
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [&, error, socket]()
		{
			if (!IsClosing)
				disconnect_socket_after_error(error, socket);
		});
	}
	
	if (TCP.acceptor.is_open())
	{
		socket_ptr conn_socket = MakeShared<asio::ip::tcp::socket>(TCP.context);
		TCP.acceptor.async_accept(
			*conn_socket, std::bind(&UWSServer::accept, this, asio::placeholders::error, conn_socket));
	}
}

void UWSServer::read_handshake(const std::error_code& error, const size_t bytes_recvd, const socket_ptr& socket)
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

	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
	});
	std::istream response_stream(ListenerBuffers[socket].Get());
	std::string method;
	response_stream >> method;
	std::string path;
	response_stream >> path;
	std::string version;
	response_stream >> version;

	std::string error_payload;
	if (method != "GET")
	{
		package_handshake_error(405, "", socket);
		return;
	}
	if (version != "HTTP/1.1" && version != "HTTP/2.0")
	{
		package_handshake_error(505, "", socket);
		return;
	}

	FServerRequest request;
	version.erase(0, 5);
	request.Version = UTF8_TO_TCHAR(version.c_str());
	request.Method = EMethod::GET;
	request.Path = UTF8_TO_TCHAR(path.c_str());
	if (ListenerBuffers[socket]->size() <= 2)
	{
		package_handshake_error(400, "Invalid handshake.", socket);
		return;
	}
	ListenerBuffers[socket]->consume(2);
	asio::async_read_until(
		*socket, *ListenerBuffers[socket], "\r\n\r\n",
		std::bind(&UWSServer::read_headers, this, asio::placeholders::error, request, socket));
}

void UWSServer::read_headers(const std::error_code& error, FServerRequest request, const socket_ptr& socket)
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
	std::istream response_stream(ListenerBuffers[socket].Get());
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
	{
		UHttpFunctionLibrary::ServerAppendHeader(request, UTF8_TO_TCHAR(header.c_str()));
	}
	for (const TPair<FString, FString>& pair : RequestHandshake.Headers)
		UE_LOG(LogTemp, Error, TEXT("%s"), *pair.Key);
	consume_listening_buffer(socket);
	FServerResponse response = ResponseHandshake;
	response.Version = request.Version;
	if (!request.Headers.Contains("Connection"))
	{
		package_handshake_error(400, "Invalid handshake: \"Connection\" header is not set.", socket);
		return;
	}
	if (request.Headers["Connection"] != "Upgrade")
	{
		package_handshake_error(400, "Invalid handshake: \"Connection\" header must be \"Upgrade\".", socket);
		return;
	}
	if (!request.Headers.Contains("Upgrade"))
	{
		package_handshake_error(400, "Invalid handshake: \"Upgrade\" header is not set.", socket);
		return;
	}
	if (request.Headers["Upgrade"] != "websocket")
	{
		package_handshake_error(400, "Invalid handshake: \"Upgrade\" header must be \"websocket\".", socket);
		return;
	}
	if (!request.Headers.Contains("Sec-WebSocket-Version"))
	{
		package_handshake_error(400, "Invalid handshake: \"Sec-WebSocket-Version\" header is not set.", socket);
		return;
	}
	if (request.Headers["Sec-WebSocket-Version"] != "13")
	{
		package_handshake_error(400, "Invalid handshake: \"Sec-WebSocket-Version\" header must be \"13\".", socket);
		return;
	}
	if (OnSocketAccepted.IsBound())
	{
		AsyncTask(ENamedThreads::GameThread, [&, request, response, socket]()
		{
			OnSocketAccepted.Broadcast(request, response, socket);
		});
	}
	else
	{
		package_handshake(request, response, socket);
	}
}

void UWSServer::write_handshake(const std::error_code& error, const size_t bytes_sent, const socket_ptr& socket,
                                const uint32_t status_code)
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
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
	});
	if (status_code != 101)
	{
		if (socket->is_open() || !IsClosing)
		{
			AsyncTask(ENamedThreads::GameThread, [&, socket]()
			{
				DisconnectSocket(socket);
			});
		}

		return;
	}
	asio::async_read(
		*socket, *ListenerBuffers[socket], asio::transfer_at_least(1),
		std::bind(&UWSServer::read, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred, socket));
}

void UWSServer::write(const std::error_code& error, const size_t bytes_sent, const socket_ptr& socket)
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
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent, error]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
		OnMessageSent.Broadcast(error, socket);
	});
}

void UWSServer::read(const std::error_code& error, const size_t bytes_recvd, const socket_ptr& socket)
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
	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
	});
	FWsMessage rDataFrame;
	if (!decode_payload(rDataFrame, socket))
	{
		consume_listening_buffer(socket);
		asio::async_read(
			*socket, *ListenerBuffers[socket], asio::transfer_at_least(1),
			std::bind(&UWSServer::read, this, asio::placeholders::error,
			          asio::placeholders::bytes_transferred, socket));
		return;
	}
	if (rDataFrame.DataFrame.opcode == EOpcode::PING)
	{
		TArray<uint8> pong_buffer = {'p', 'o', 'n', 'g', '\0'};
		post_buffer(EOpcode::PONG, pong_buffer, socket);
	}
	else if (rDataFrame.DataFrame.opcode == EOpcode::PONG)
	{
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnPongReceived.Broadcast();
		});
	}
	else if (rDataFrame.DataFrame.opcode == EOpcode::CONNECTION_CLOSE)
	{
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnCloseNotify.Broadcast();
		});
	}
	else
	{
		rDataFrame.Size = bytes_recvd;
		AsyncTask(ENamedThreads::GameThread, [&, rDataFrame, socket]()
		{
			OnMessageReceived.Broadcast(rDataFrame, socket);
		});
	}

	consume_listening_buffer(socket);
	asio::async_read(
		*socket, *ListenerBuffers[socket], asio::transfer_at_least(1),
		std::bind(&UWSServer::read, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred, socket));
}

bool UWSServerSsl::SendHandshakeTo(const FServerRequest& Request, FServerResponse Response,
                                   const FTCPSslSocket& SslSocket)
{
	if (!SslSocket.SmartPtr->next_layer().is_open())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UWSServerSsl::package_handshake, this, Request, Response, SslSocket.SmartPtr, 101));
	return true;
}

bool UWSServerSsl::SendHandshakeErrorTo(const int StatusCode, const FString& Why, const FTCPSslSocket& SslSocket)
{
	if (!SslSocket.SmartPtr->next_layer().is_open())
	{
		return false;
	}

	asio::post(GetThreadPool(),
	           std::bind(&UWSServerSsl::package_handshake_error, this, StatusCode, Why, SslSocket.SmartPtr));
	return true;
}

bool UWSServerSsl::SendStrTo(const FString& Message, const FTCPSslSocket& SslSocket)
{
	if (!SslSocket.SmartPtr->next_layer().is_open() || Message.IsEmpty())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UWSServerSsl::package_string, this, Message, SslSocket.SmartPtr));
	return true;
}

bool UWSServerSsl::SendBufferTo(const TArray<uint8> Buffer, const FTCPSslSocket& SslSocket)
{
	if (!SslSocket.SmartPtr->next_layer().is_open() || Buffer.Num() == 0)
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UWSServerSsl::package_buffer, this, Buffer, SslSocket.SmartPtr));
	return true;
}

bool UWSServerSsl::SendPingTo(const FTCPSslSocket& SslSocket)
{
	if (!SslSocket.SmartPtr->next_layer().is_open())
	{
		return false;
	}

	TArray<uint8> ping_buffer = {'p', 'i', 'n', 'g', '\0'};
	asio::post(GetThreadPool(), std::bind(&UWSServerSsl::post_buffer, this,
	                                      EOpcode::PING, ping_buffer, SslSocket.SmartPtr));
	return true;
}

bool UWSServerSsl::Open()
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

	asio::post(GetThreadPool(), std::bind(&UWSServerSsl::run_context_thread, this));
	return true;
}

void UWSServerSsl::Close()
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
	if (ListenerBuffers.Num() > 0)
	{
		ListenerBuffers.Empty();
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

void UWSServerSsl::DisconnectSocket(const FTCPSslSocket& SslSocket)
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
	if (ListenerBuffers.Contains(SslSocket.SmartPtr))
	{
		ListenerBuffers.Remove(SslSocket.SmartPtr);
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

void UWSServerSsl::disconnect_socket_after_error(const asio::error_code& error, const ssl_socket_ptr& ssl_socket)
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
	if (ListenerBuffers.Contains(ssl_socket))
	{
		ListenerBuffers.Remove(ssl_socket);
	}
	if (TCP.ssl_sockets.Contains(ssl_socket))
	{
		TCP.ssl_sockets.Remove(ssl_socket);
	}
	OnSocketDisconnected.Broadcast(error, ssl_socket);
}

void UWSServerSsl::post_string(const FString& str, const ssl_socket_ptr& ssl_socket)
{
	FScopeLock Guard(&MutexBuffer);
	SDataFrame.opcode = EOpcode::TEXT_FRAME;
	package_string(str, ssl_socket);
}

void UWSServerSsl::post_buffer(EOpcode opcode, const TArray<uint8>& buffer, const ssl_socket_ptr& ssl_socket)
{
	FScopeLock Guard(&MutexBuffer);
	SDataFrame.opcode = opcode;
	if (opcode == EOpcode::BINARY_FRAME)
	{
		package_buffer(buffer, ssl_socket);
	}
	else if (opcode == EOpcode::PING || opcode == EOpcode::PONG)
	{
		std::vector<uint8> p_buffer(buffer.GetData(), buffer.GetData() + buffer.Num());
		encode_buffer_payload(p_buffer);
		asio::async_write(*ssl_socket, asio::buffer(p_buffer.data(), p_buffer.size()),
		                  std::bind(&UWSServerSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, ssl_socket));
	}
}

void UWSServerSsl::package_string(const FString& str, const ssl_socket_ptr& ssl_socket)
{
	std::string payload;
	const int frame_encode_size = static_cast<int>(get_frame_encode_size(str.Len()));
	if (!SplitBuffer || str.Len() + frame_encode_size <= MaxSendBufferSize)
	{
		SDataFrame.fin = true;
		payload = TCHAR_TO_UTF8(*str);
		payload = encode_string_payload(payload);
		asio::async_write(*ssl_socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWSServerSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, ssl_socket));
		return;
	}

	SDataFrame.fin = true;
	size_t string_offset = 0;
	const size_t max_size = MaxSendBufferSize - get_frame_encode_size(str.Len());
	const size_t str_len = static_cast<size_t>(str.Len());
	while (string_offset < str_len)
	{
		SDataFrame.fin = string_offset < str_len;
		size_t package_size = std::min(max_size, str_len - string_offset);
		FString strshrink = str.Mid(string_offset, package_size);
		payload = TCHAR_TO_UTF8(*strshrink);
		payload = encode_string_payload(payload);
		asio::async_write(*ssl_socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWSServerSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, ssl_socket));
		string_offset += package_size;
		if (SDataFrame.opcode != EOpcode::FRAME_CON)
		{
			SDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::string UWSServerSsl::encode_string_payload(const std::string& payload)
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

void UWSServerSsl::package_buffer(const TArray<uint8>& buffer, const ssl_socket_ptr& ssl_socket)
{
	std::vector<uint8> payload;
	const int frame_encode_size = static_cast<int>(get_frame_encode_size(buffer.Num()));
	if (!SplitBuffer || buffer.Num() + frame_encode_size <= MaxSendBufferSize)
	{
		payload.assign(buffer.GetData(), buffer.GetData() + buffer.Num());
		SDataFrame.fin = true;
		encode_buffer_payload(payload);
		asio::async_write(*ssl_socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWSServerSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, ssl_socket));
		return;
	}

	SDataFrame.fin = false;
	size_t buffer_offset = 0;
	const size_t max_size = MaxSendBufferSize - get_frame_encode_size(buffer.Num());
	const size_t buf_len = static_cast<size_t>(buffer.Num());
	while (buffer_offset < buf_len)
	{
		SDataFrame.fin = buffer_offset < buf_len;
		size_t package_size = std::min(max_size, buf_len - buffer_offset);
		payload.assign(buffer.GetData() + buffer_offset, buffer.GetData() + buffer_offset + package_size);
		encode_buffer_payload(payload);
		asio::async_write(*ssl_socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWSServerSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred, ssl_socket));
		buffer_offset += package_size;
		if (SDataFrame.opcode != EOpcode::FRAME_CON)
		{
			SDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::vector<uint8> UWSServerSsl::encode_buffer_payload(const std::vector<uint8>& payload)
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

size_t UWSServerSsl::get_frame_encode_size(const size_t buffer_size)
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

std::array<uint8, 4> UWSServerSsl::mask_gen()
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

bool UWSServerSsl::decode_payload(FWsMessage& data_frame, const ssl_socket_ptr& ssl_socket)
{
	if (ListenerBuffers[ssl_socket]->size() < 2)
	{
		return false;
	}

	size_t size = ListenerBuffers[ssl_socket]->size();
	std::vector<uint8> encoded_buffer;
	encoded_buffer.resize(size);
	asio::buffer_copy(asio::buffer(encoded_buffer, encoded_buffer.size()), ListenerBuffers[ssl_socket]->data());
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

TArray<uint8> UWSServerSsl::sha1(const FString& input)
{
	const uint32_t h0 = 0x67452301;
	const uint32_t h1 = 0xEFCDAB89;
	const uint32_t h2 = 0x98BADCFE;
	const uint32_t h3 = 0x10325476;
	const uint32_t h4 = 0xC3D2E1F0;

	uint64_t bit_length = input.Len() * 8;
	std::string padded_input = TCHAR_TO_UTF8(*input);

	padded_input += static_cast<char>(0x80);

	while ((padded_input.size() % 64) != 56)
	{
		padded_input += static_cast<char>(0x00);
	}

	for (int i = 7; i >= 0; --i)
	{
		padded_input += static_cast<char>((bit_length >> (i * 8)) & 0xFF);
	}

	uint32_t h[5] = {h0, h1, h2, h3, h4};

	for (size_t i = 0; i < padded_input.size(); i += 64)
	{
		uint32_t w[80] = {0};

		for (int j = 0; j < 16; ++j)
		{
			w[j] = static_cast<uint8_t>(padded_input[i + j * 4]) << 24 |
				static_cast<uint8_t>(padded_input[i + j * 4 + 1]) << 16 |
				static_cast<uint8_t>(padded_input[i + j * 4 + 2]) << 8 |
				static_cast<uint8_t>(padded_input[i + j * 4 + 3]);
		}

		for (int j = 16; j < 80; ++j)
		{
			w[j] = (w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16]);
			w[j] = (w[j] << 1) | (w[j] >> 31);
		}

		uint32_t a = h[0];
		uint32_t b = h[1];
		uint32_t c = h[2];
		uint32_t d = h[3];
		uint32_t e = h[4];

		for (int j = 0; j < 80; ++j)
		{
			uint32_t f, k;

			if (j < 20)
			{
				f = (b & c) | ((~b) & d);
				k = 0x5A827999;
			}
			else if (j < 40)
			{
				f = b ^ c ^ d;
				k = 0x6ED9EBA1;
			}
			else if (j < 60)
			{
				f = (b & c) | (b & d) | (c & d);
				k = 0x8F1BBCDC;
			}
			else
			{
				f = b ^ c ^ d;
				k = 0xCA62C1D6;
			}

			uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[j];
			e = d;
			d = c;
			c = (b << 30) | (b >> 2);
			b = a;
			a = temp;
		}

		h[0] += a;
		h[1] += b;
		h[2] += c;
		h[3] += d;
		h[4] += e;
	}

	TArray<uint8> hash;
	hash.SetNum(20);
	for (int i = 0; i < 5; ++i)
	{
		hash[i * 4] = (h[i] >> 24) & 0xFF;
		hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
		hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
		hash[i * 4 + 3] = h[i] & 0xFF;
	}

	return hash;
}

FString UWSServerSsl::base64_encode(const uint8_t* input, size_t length)
{
	const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::string result;
	int i = 0;
	uint32_t octet_a, octet_b, octet_c, triple;

	while (length >= 3)
	{
		octet_a = input[i++];
		octet_b = input[i++];
		octet_c = input[i++];

		triple = (octet_a << 16) + (octet_b << 8) + octet_c;

		result.push_back(base64_chars[(triple >> 18) & 0x3F]);
		result.push_back(base64_chars[(triple >> 12) & 0x3F]);
		result.push_back(base64_chars[(triple >> 6) & 0x3F]);
		result.push_back(base64_chars[triple & 0x3F]);

		length -= 3;
	}

	if (length > 0)
	{
		octet_a = input[i++];
		octet_b = length > 1 ? input[i++] : 0;

		triple = (octet_a << 16) + (octet_b << 8);

		result.push_back(base64_chars[(triple >> 18) & 0x3F]);
		result.push_back(base64_chars[(triple >> 12) & 0x3F]);
		result.push_back(length == 2 ? base64_chars[(triple >> 6) & 0x3F] : '=');
		result.push_back('=');
	}

	return UTF8_TO_TCHAR(result.c_str());
}

FString UWSServerSsl::generate_accept_key(const FString& sec_websocket_key)
{
	const FString magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	FString concatenated = sec_websocket_key + magic_string;
	auto hash = sha1(concatenated);
	return base64_encode(hash.GetData(), hash.Num());
}

void UWSServerSsl::package_handshake(const FServerRequest& req, FServerResponse& res, const ssl_socket_ptr& ssl_socket,
                                     const uint32_t status_code)
{
	if (req.Headers.Contains("Sec-WebSocket-Key"))
	{
		FString key = req.Headers["Sec-WebSocket-Key"];
		FString accept_key = generate_accept_key(key);
		res.Headers.Add("Sec-WebSocket-Accept", accept_key);
	}
	if (req.Headers.Contains("Sec-WebSocket-Protocol"))
	{
		FString protocol = req.Headers["Sec-WebSocket-Protocol"];
		if (protocol.Find("chat") != INDEX_NONE || protocol.Find("superchat") != INDEX_NONE)
		{
			res.Headers.Add("Sec-WebSocket-Protocol", "chat");
		}
		else if (protocol.Find("json") != INDEX_NONE)
		{
			res.Headers.Add("Sec-WebSocket-Protocol", "json");
		}
		else if (protocol.Find("xml") != INDEX_NONE)
		{
			res.Headers.Add("Sec-WebSocket-Protocol", "xml");
		}
	}
	FString payload = "HTTP/" + res.Version + " 101 Switching Protocols\r\n";
	if (!res.Headers.Num() == 0)
	{
		for (const TPair<FString, FString> header : res.Headers)
		{
			payload += header.Key + ": " + header.Value + "\r\n";
		}
	}
	payload += "\r\n";
	std::string utf8payload = TCHAR_TO_UTF8(*payload);
	asio::async_write(
		*ssl_socket, asio::buffer(utf8payload.data(), utf8payload.size()),
		std::bind(&UWSServerSsl::write_handshake, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred, ssl_socket, status_code));
}

void UWSServerSsl::package_handshake_error(const uint32_t status_code, const FString& body, const ssl_socket_ptr& ssl_socket)
{
	std::string payload;
	if (ResponseStatusCode.Contains(status_code))
	{
		payload = "HTTP/1.1 " + std::to_string(status_code) + " " + TCHAR_TO_UTF8(*ResponseStatusCode[status_code]) +
			"\r\n";
	}
	else
	{
		payload = "HTTP/1.1 400 HTTP Bad Request\r\n";
	}
	std::string body_content = body.IsEmpty() ? TCHAR_TO_UTF8(*body) : "";
	std::string len = std::to_string(body_content.length());
	switch (status_code)
	{
	case 400:
		payload += "Content-Type: text/plain\r\n"
		"Content-Length: " + len + "\r\n"
		"Connection: close\r\n"
		"\r\n";
		payload += body_content;
		payload += "\r\n";
		break;
	case 405:
		payload += "Allow: GET\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n\r\n";
		break;
	case 505:
		payload += "Content-Type: text/plain\r\n"
		"Content-Length: 27\r\n"
		"Connection: close\r\n"
		"\r\n"
		"HTTP version not supported.\r\n";
		break;
	default: payload += "\r\n";
	}
	asio::async_write(
		*ssl_socket, asio::buffer(payload.data(), payload.size()),
		std::bind(&UWSServerSsl::write_handshake, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred, ssl_socket, status_code));
}

void UWSServerSsl::consume_listening_buffer(const ssl_socket_ptr& ssl_socket)
{
	const size_t res_size = ListenerBuffers[ssl_socket]->size();
	if (res_size > 0)
	{
		ListenerBuffers[ssl_socket]->consume(res_size);
	}
}

void UWSServerSsl::run_context_thread()
{
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	ssl_socket_ptr ssl_conn_socket = MakeShared<asio::ssl::stream<asio::ip::tcp::socket>>(TCP.context, TCP.ssl_context);
	TCP.acceptor.async_accept(ssl_conn_socket->lowest_layer(),
	                          std::bind(&UWSServerSsl::accept, this, asio::placeholders::error,
	                                    ssl_conn_socket));
	TCP.context.run();
	if (!IsClosing)
	{
		Close();
	}
}

void UWSServerSsl::accept(const asio::error_code& error, ssl_socket_ptr& ssl_socket)
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
				std::bind(&UWSServerSsl::accept, this, asio::placeholders::error, ssl_conn_socket));
		}
		return;
	}

	if (TCP.ssl_sockets.Num() < Backlog)
	{
		ssl_socket->async_handshake(asio::ssl::stream_base::server,
								std::bind(&UWSServerSsl::ssl_handshake, this,
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
			std::bind(&UWSServerSsl::accept, this, asio::placeholders::error, ssl_conn_socket));
	}
}

void UWSServerSsl::ssl_handshake(const asio::error_code& error, ssl_socket_ptr& ssl_socket)
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
	TSharedPtr<asio::streambuf> listening_buffer = MakeShared<asio::streambuf>();
	ListenerBuffers.Add(ssl_socket, listening_buffer);
	asio::async_read_until(*ssl_socket, *listening_buffer, "\r\n",
						   std::bind(&UWSServerSsl::read_handshake, this,
									 asio::placeholders::error,
									 asio::placeholders::bytes_transferred, ssl_socket));
}

void UWSServerSsl::read_handshake(const std::error_code& error, const size_t bytes_recvd,
	const ssl_socket_ptr& ssl_socket)
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

	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
	});
	std::istream response_stream(ListenerBuffers[ssl_socket].Get());
	std::string method;
	response_stream >> method;
	std::string path;
	response_stream >> path;
	std::string version;
	response_stream >> version;

	std::string error_payload;
	if (method != "GET")
	{
		package_handshake_error(405, "", ssl_socket);
		return;
	}
	if (version != "HTTP/1.1" && version != "HTTP/2.0")
	{
		package_handshake_error(505, "", ssl_socket);
		return;
	}

	FServerRequest request;
	version.erase(0, 5);
	request.Version = UTF8_TO_TCHAR(version.c_str());
	request.Method = EMethod::GET;
	request.Path = UTF8_TO_TCHAR(path.c_str());
	if (ListenerBuffers[ssl_socket]->size() <= 2)
	{
		package_handshake_error(400, "Invalid handshake.", ssl_socket);
		return;
	}
	ListenerBuffers[ssl_socket]->consume(2);
	asio::async_read_until(
		*ssl_socket, *ListenerBuffers[ssl_socket], "\r\n\r\n",
		std::bind(&UWSServerSsl::read_headers, this, asio::placeholders::error, request, ssl_socket));
}

void UWSServerSsl::read_headers(const std::error_code& error, FServerRequest request, const ssl_socket_ptr& ssl_socket)
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
	std::istream response_stream(ListenerBuffers[ssl_socket].Get());
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
	{
		UHttpFunctionLibrary::ServerAppendHeader(request, UTF8_TO_TCHAR(header.c_str()));
	}
	consume_listening_buffer(ssl_socket);
	FServerResponse response = ResponseHandshake;
	response.Version = request.Version;
	if (!request.Headers.Contains("Connection"))
	{
		package_handshake_error(400, "Invalid handshake: \"Connection\" header is not set.", ssl_socket);
		return;
	}
	if (request.Headers["Connection"] != "Upgrade")
	{
		package_handshake_error(400, "Invalid handshake: \"Connection\" header must be \"Upgrade\".", ssl_socket);
		return;
	}
	if (!request.Headers.Contains("Upgrade"))
	{
		package_handshake_error(400, "Invalid handshake: \"Upgrade\" header is not set.", ssl_socket);
		return;
	}
	if (request.Headers["Upgrade"] != "websocket")
	{
		package_handshake_error(400, "Invalid handshake: \"Upgrade\" header must be \"websocket\".", ssl_socket);
		return;
	}
	if (!request.Headers.Contains("Sec-WebSocket-Version"))
	{
		package_handshake_error(400, "Invalid handshake: \"Sec-WebSocket-Version\" header is not set.", ssl_socket);
		return;
	}
	if (request.Headers["Sec-WebSocket-Version"] != "13")
	{
		package_handshake_error(400, "Invalid handshake: \"Sec-WebSocket-Version\" header must be \"13\".", ssl_socket);
		return;
	}
		
	if (OnSocketAccepted.IsBound())
	{
		AsyncTask(ENamedThreads::GameThread, [&, request, response, ssl_socket]()
		{
			OnSocketAccepted.Broadcast(request, response, ssl_socket);
		});
	}
	else
	{
		package_handshake(request, response, ssl_socket);
	}
}

void UWSServerSsl::write_handshake(const std::error_code& error, const size_t bytes_sent,
	const ssl_socket_ptr& ssl_socket, const uint32_t status_code)
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
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
	});
	if (status_code != 101)
	{
		if (ssl_socket->next_layer().is_open() || !IsClosing)
		{
			AsyncTask(ENamedThreads::GameThread, [&, ssl_socket]()
			{
				DisconnectSocket(ssl_socket);
			});
		}

		return;
	}
	asio::async_read(
		*ssl_socket, *ListenerBuffers[ssl_socket], asio::transfer_at_least(1),
		std::bind(&UWSServerSsl::read, this, asio::placeholders::error,
				  asio::placeholders::bytes_transferred, ssl_socket));
}

void UWSServerSsl::write(const std::error_code& error, const size_t bytes_sent, const ssl_socket_ptr& ssl_socket)
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
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent, error]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
		OnMessageSent.Broadcast(error, ssl_socket);
	});
}

void UWSServerSsl::read(const std::error_code& error, const size_t bytes_recvd, const ssl_socket_ptr& ssl_socket)
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
	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
	});
	FWsMessage rDataFrame;
	if (!decode_payload(rDataFrame, ssl_socket))
	{
		consume_listening_buffer(ssl_socket);
		asio::async_read(
			*ssl_socket, *ListenerBuffers[ssl_socket], asio::transfer_at_least(1),
			std::bind(&UWSServerSsl::read, this, asio::placeholders::error,
					  asio::placeholders::bytes_transferred, ssl_socket));
		return;
	}
	if (rDataFrame.DataFrame.opcode == EOpcode::PING)
	{
		TArray<uint8> pong_buffer = {'p', 'o', 'n', 'g', '\0'};
		post_buffer(EOpcode::PONG, pong_buffer, ssl_socket);
	}
	else if (rDataFrame.DataFrame.opcode == EOpcode::PONG)
	{
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnPongReceived.Broadcast();
		});
	}
	else if (rDataFrame.DataFrame.opcode == EOpcode::CONNECTION_CLOSE)
	{
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnCloseNotify.Broadcast();
		});
	}
	else
	{
		rDataFrame.Size = bytes_recvd;
		AsyncTask(ENamedThreads::GameThread, [&, rDataFrame, ssl_socket]()
		{
			OnMessageReceived.Broadcast(rDataFrame, ssl_socket);
		});
	}

	consume_listening_buffer(ssl_socket);
	asio::async_read(
		*ssl_socket, *ListenerBuffers[ssl_socket], asio::transfer_at_least(1),
		std::bind(&UWSServerSsl::read, this, asio::placeholders::error,
				  asio::placeholders::bytes_transferred, ssl_socket));
}
