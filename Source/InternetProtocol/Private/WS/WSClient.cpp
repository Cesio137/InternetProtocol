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

#include "WS/WSClient.h"

bool UWSClient::SendStr(const FString& Message)
{
	if (!TCP.socket.is_open() || Message.IsEmpty())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UWSClient::post_string, this, Message));
	return true;
}

bool UWSClient::SendBuffer(const TArray<uint8> Buffer)
{
	if (!TCP.socket.is_open() || Buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UWSClient::post_buffer, this, EOpcode::BINARY_FRAME, Buffer));
	return true;
}

bool UWSClient::SendPing()
{
	if (!TCP.socket.is_open())
	{
		return false;
	}

	TArray<uint8> ping_buffer = {'p', 'i', 'n', 'g', '\0'};
	asio::post(GetThreadPool(), std::bind(&UWSClient::post_buffer, this, EOpcode::PING, ping_buffer));
	return true;
}

bool UWSClient::Connect()
{
	if (TCP.socket.is_open())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UWSClient::run_context_thread, this));
	return true;
}

void UWSClient::Close()
{
	IsClosing = true;
	TCP.context.stop();
	if (TCP.socket.is_open())
	{
		FScopeLock Guard(&MutexError);
		TCP.socket.shutdown(asio::ip::udp::socket::shutdown_both, ErrorCode);
		if (ErrorCode)
		{
			OnError.Broadcast(ErrorCode);
		}
		TCP.socket.close(ErrorCode);
		if (ErrorCode)
		{
			OnError.Broadcast(ErrorCode);
		}
	}
	TCP.context.restart();
	OnClose.Broadcast();
	IsClosing = false;
}

void UWSClient::post_string(const FString& str)
{
	FScopeLock Guard(&MutexBuffer);
	SDataFrame.opcode = EOpcode::TEXT_FRAME;
	package_string(str);
}

void UWSClient::post_buffer(EOpcode opcode, const TArray<uint8>& buffer)
{
	FScopeLock Guard(&MutexBuffer);
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
		                  std::bind(&UWSClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred));
	}
}

void UWSClient::package_string(const FString& str)
{
	std::string payload;
	const int frame_encode_size = static_cast<int>(get_frame_encode_size(str.Len()));
	if (!SplitBuffer || str.Len() + frame_encode_size <= MaxSendBufferSize)
	{
		SDataFrame.fin = true;
		payload = TCHAR_TO_UTF8(*str);
		payload = encode_string_payload(payload);
		asio::async_write(TCP.socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWSClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
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
		asio::async_write(TCP.socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWSClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		string_offset += package_size;
		if (SDataFrame.opcode != EOpcode::FRAME_CON)
		{
			SDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::string UWSClient::encode_string_payload(const std::string& payload)
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

void UWSClient::package_buffer(const TArray<uint8>& buffer)
{
	std::vector<uint8> payload;
	const int frame_encode_size = static_cast<int>(get_frame_encode_size(buffer.Num()));
	if (!SplitBuffer || buffer.Num() + frame_encode_size <= MaxSendBufferSize)
	{
		payload.assign(buffer.GetData(), buffer.GetData() + buffer.Num());
		SDataFrame.fin = true;
		encode_buffer_payload(payload);
		asio::async_write(TCP.socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWSClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
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
		asio::async_write(TCP.socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWSClient::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
		if (SDataFrame.opcode != EOpcode::FRAME_CON)
		{
			SDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::vector<uint8> UWSClient::encode_buffer_payload(const std::vector<uint8>& payload)
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

size_t UWSClient::get_frame_encode_size(const size_t buffer_size)
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

std::array<uint8, 4> UWSClient::mask_gen()
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

bool UWSClient::decode_payload(FWsMessage& data_frame)
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

TArray<uint8> UWSClient::sha1(const FString& input)
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

FString UWSClient::base64_encode(const uint8* input, size_t length)
{
	const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::string result;
	int i = 0;
	uint32_t octet_a, octet_b, octet_c, triple;

	while (length >= 3) {
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

	if (length > 0) {
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

FString UWSClient::generate_accept_key(const FString& sec_websocket_key)
{
	const FString magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	FString concatenated = sec_websocket_key + magic_string;
	auto hash = sha1(concatenated);
	return base64_encode(hash.GetData(), hash.Num());
}

void UWSClient::consume_response_buffer()
{
	const size_t res_size = ResponseBuffer.size();
	if (res_size > 0)
	{
		ResponseBuffer.consume(res_size);
	}
}

void UWSClient::run_context_thread()
{
	FScopeLock Guard(&MutexError);
	ErrorCode.clear();
	TCP.context.restart();
	TCP.resolver.async_resolve(TCHAR_TO_UTF8(*Host), TCHAR_TO_UTF8(*Service),
	                           std::bind(&UWSClient::resolve, this, asio::placeholders::error,
	                                     asio::placeholders::endpoint));
	TCP.context.run();
	if (!IsClosing)
	{
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			Close();
		});
	}
}

void UWSClient::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	TCP.endpoints = endpoints;
	asio::async_connect(TCP.socket, TCP.endpoints,
	                    std::bind(&UWSClient::conn, this, asio::placeholders::error)
	);
}

void UWSClient::conn(const std::error_code& error)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	FString request;
	request = "GET " + RequestHandshake.Path + " HTTP/" + RequestHandshake.Version + "\r\n";
	if (!RequestHandshake.Headers.Contains("Host"))
	{
		request += "Host: " + Host + ":" + Service + "\r\n";
	}
	if (!RequestHandshake.Headers.Num() == 0)
	{
		for (const auto& header : RequestHandshake.Headers)
		{
			request += header.Key + ": " + header.Value + "\r\n";
		}
	}
	request += "\r\n";
	std::string payload = TCHAR_TO_UTF8(*request);
	asio::async_write(TCP.socket, asio::buffer(payload.data(), payload.size()),
	                  std::bind(&UWSClient::write_handshake, this, asio::placeholders::error,
	                            asio::placeholders::bytes_transferred));
}

void UWSClient::write_handshake(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
	});
	asio::async_read_until(TCP.socket, ResponseBuffer, "\r\n",
	                       std::bind(&UWSClient::read_handshake, this, asio::placeholders::error,
	                                 asio::placeholders::bytes_transferred));
}

void UWSClient::read_handshake(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
	});
	UHttpFunctionLibrary::ClientClearResponse(ResponseHandshake);
	std::istream response_stream(&ResponseBuffer);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		ResponseHandshake.StatusCode = 505;
		ResponseHandshake.Body = "Invalid handshake: HTTP Version Not Supported.";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
			consume_response_buffer();
		});
		return;
	}
	ResponseHandshake.StatusCode = status_code;
	if (ResponseBuffer.size() == 0)
	{
		ResponseHandshake.Body = "Invalid handshake: " + ResponseStatusCode[status_code] + ".";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
			consume_response_buffer();
		});
		return;
	}
	asio::async_read_until(TCP.socket, ResponseBuffer, "\r\n\r\n",
	                       std::bind(&UWSClient::read_headers, this, asio::placeholders::error));
}

void UWSClient::read_headers(const std::error_code& error)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	UHttpFunctionLibrary::ClientClearResponse(ResponseHandshake);
	std::istream response_stream(&ResponseBuffer);
	std::string header;
	while (std::getline(response_stream, header) && header != "\r")
	{
		UHttpFunctionLibrary::ClientAppendHeader(ResponseHandshake, UTF8_TO_TCHAR(header.c_str()));
	}
	if (ResponseHandshake.Headers.Num() == 0) {
		ResponseHandshake.StatusCode = ResponseHandshake.StatusCode != 101 ? ResponseHandshake.StatusCode : 400;
		ResponseHandshake.Body = ResponseHandshake.StatusCode != 101
								 ? "Invalid handshake: " + ResponseStatusCode[ResponseHandshake.StatusCode]
								 : "Invalid handshake: Header is empty.";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
		});
		consume_response_buffer();
		return;
	}
	if (!ResponseHandshake.Headers.Contains("Connection") ||
		!ResponseHandshake.Headers.Contains("Upgrade") ||
		!ResponseHandshake.Headers.Contains("Sec-WebSocket-Accept")) {
		if (ResponseBuffer.size() > 0) {
			std::ostringstream body_buffer;
			body_buffer << &ResponseBuffer;
			UHttpFunctionLibrary::ClientSetBody(ResponseHandshake, UTF8_TO_TCHAR(body_buffer.str().c_str()));
		}
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
		});
		consume_response_buffer();
		return;
	}
	consume_response_buffer();
	if (ResponseHandshake.Headers["Connection"] != "Upgrade") {
		ResponseHandshake.Body = "Invalid handshake: \"Connection\" must be \"Upgrade\".";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
		});
		return;
	}
	if (ResponseHandshake.Headers["Upgrade"] != "websocket") {
		ResponseHandshake.Body = "Invalid handshake: \"Upgrade\" must be \"websocket\".";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
		});
		return;
	}
	FString protocol = RequestHandshake.Headers["Sec-WebSocket-Protocol"];
	FString res_protocol = ResponseHandshake.Headers["Sec-WebSocket-Protocol"];
	if (protocol.Find(res_protocol) == INDEX_NONE) {
		ResponseHandshake.Body = "Invalid handshake: \"Sec-WebSocket-Protocol\" must be \"" + protocol +
							 "\" or contain one of them.";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
		});
		return;
	}
	FString accept_key = ResponseHandshake.Headers["Sec-WebSocket-Accept"];
	FString encoded_key = generate_accept_key(RequestHandshake.Headers["Sec-WebSocket-Key"]);
	if (accept_key != encoded_key) {
		ResponseHandshake.Body = "Invalid handshake: \"Sec-WebSocket-Accept\" is invalid.";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		OnConnected.Broadcast(ResponseHandshake);
	});

	asio::async_read(
		TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
		std::bind(&UWSClient::read, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred));
}

void UWSClient::write(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
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

void UWSClient::read(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
	});
	FWsMessage rDataFrame;
	if (!decode_payload(rDataFrame))
	{
		ResponseBuffer.consume(ResponseBuffer.size());
		asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
		                 std::bind(&UWSClient::read, this, asio::placeholders::error,
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
		AsyncTask(ENamedThreads::GameThread, [&, rDataFrame]()
		{
			OnMessageReceived.Broadcast(rDataFrame);
		});
	}
	consume_response_buffer();
	asio::async_read(TCP.socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UWSClient::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
}

bool UWSClientSsl::SendStr(const FString& message)
{
	if (!TCP.ssl_socket.next_layer().is_open() || message.IsEmpty())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UWSClientSsl::post_string, this, message));
	return true;
}

bool UWSClientSsl::SendBuffer(const TArray<uint8> buffer)
{
	if (!TCP.ssl_socket.next_layer().is_open() || buffer.Num() <= 0)
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UWSClientSsl::post_buffer, this, EOpcode::BINARY_FRAME, buffer));
	return true;
}

bool UWSClientSsl::SendPing()
{
	if (!TCP.ssl_socket.lowest_layer().is_open())
	{
		return false;
	}

	TArray<uint8> ping_buffer = {'p', 'i', 'n', 'g', '\0'};
	asio::post(GetThreadPool(), std::bind(&UWSClientSsl::post_buffer, this, EOpcode::PONG, ping_buffer));
	return true;
}

bool UWSClientSsl::Connect()
{
	if (TCP.ssl_socket.lowest_layer().is_open())
	{
		return false;
	}

	asio::post(GetThreadPool(), std::bind(&UWSClientSsl::run_context_thread, this));
	return true;
}

void UWSClientSsl::Close()
{
	IsClosing = true;
	if (TCP.ssl_socket.next_layer().is_open())
	{
		FScopeLock Guard(&MutexError);
		TCP.ssl_socket.shutdown(ErrorCode);
		if (ErrorCode)
		{
			OnError.Broadcast(ErrorCode);
		}
		TCP.ssl_socket.next_layer().close(ErrorCode);
		if (ErrorCode)
		{
			OnError.Broadcast(ErrorCode);
		}
	}
	TCP.context.stop();
	TCP.context.restart();
	TCP.ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(TCP.context, TCP.ssl_context);
	OnClose.Broadcast();
	IsClosing = false;
}

void UWSClientSsl::post_string(const FString& str)
{
	MutexBuffer.Lock();
	SDataFrame.opcode = EOpcode::TEXT_FRAME;
	package_string(str);
	MutexBuffer.Unlock();
}

void UWSClientSsl::post_buffer(EOpcode opcode, const TArray<uint8>& buffer)
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
		                  std::bind(&UWSClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred));
	}
	MutexBuffer.Unlock();
}

void UWSClientSsl::package_string(const FString& str)
{
	std::string payload;
	const int frame_encode_size = static_cast<int>(get_frame_encode_size(str.Len()));
	if (!SplitBuffer || str.Len() + frame_encode_size <= MaxSendBufferSize)
	{
		SDataFrame.fin = true;
		payload = TCHAR_TO_UTF8(*str);
		payload = encode_string_payload(payload);
		asio::async_write(TCP.ssl_socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWSClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
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
		asio::async_write(TCP.ssl_socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWSClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		string_offset += package_size;
		if (SDataFrame.opcode != EOpcode::FRAME_CON)
		{
			SDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::string UWSClientSsl::encode_string_payload(const std::string& payload)
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

void UWSClientSsl::package_buffer(const TArray<uint8>& buffer)
{
	std::vector<uint8> payload;
	const int frame_encode_size = static_cast<int>(get_frame_encode_size(buffer.Num()));
	if (!SplitBuffer || buffer.Num() + frame_encode_size <= MaxSendBufferSize)
	{
		payload.assign(buffer.GetData(), buffer.GetData() + buffer.Num());
		SDataFrame.fin = true;
		encode_buffer_payload(payload);
		asio::async_write(TCP.ssl_socket, asio::buffer(payload.data(), payload.size()),
		                  std::bind(&UWSClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
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
		asio::async_write(TCP.ssl_socket, asio::buffer(payload, payload.size()),
		                  std::bind(&UWSClientSsl::write, this, asio::placeholders::error,
		                            asio::placeholders::bytes_transferred)
		);
		buffer_offset += package_size;
		if (SDataFrame.opcode != EOpcode::FRAME_CON)
		{
			SDataFrame.opcode = EOpcode::FRAME_CON;
		}
	}
}

std::vector<uint8> UWSClientSsl::encode_buffer_payload(const std::vector<uint8>& payload)
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

size_t UWSClientSsl::get_frame_encode_size(const size_t buffer_size)
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

std::array<uint8, 4> UWSClientSsl::mask_gen()
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

bool UWSClientSsl::decode_payload(FWsMessage& data_frame)
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

void UWSClientSsl::consume_response_buffer()
{
	const size_t res_size = ResponseBuffer.size();
	if (res_size > 0)
	{
		ResponseBuffer.consume(res_size);
	}
}

TArray<uint8> UWSClientSsl::sha1(const FString& input)
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

FString UWSClientSsl::base64_encode(const uint8_t* input, size_t length)
{
	const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::string result;
	int i = 0;
	uint32_t octet_a, octet_b, octet_c, triple;

	while (length >= 3) {
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

	if (length > 0) {
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

FString UWSClientSsl::generate_accept_key(const FString& sec_websocket_key)
{
	const FString magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	FString concatenated = sec_websocket_key + magic_string;
	auto hash = sha1(concatenated);
	return base64_encode(hash.GetData(), hash.Num());
}

void UWSClientSsl::run_context_thread()
{
	FScopeLock Guard(&MutexIO);
	ErrorCode.clear();
	TCP.context.restart();
	TCP.resolver.async_resolve(
		TCHAR_TO_UTF8(*Host), TCHAR_TO_UTF8(*Service),
		std::bind(&UWSClientSsl::resolve, this,
		          asio::placeholders::error, asio::placeholders::endpoint));
	TCP.context.run();
	if (!IsClosing)
	{
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			Close();
		});
	}
}

void UWSClientSsl::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	TCP.endpoints = endpoints;
	asio::async_connect(TCP.ssl_socket.lowest_layer(), TCP.endpoints,
	                    std::bind(&UWSClientSsl::conn, this, asio::placeholders::error)
	);
}

void UWSClientSsl::conn(const std::error_code& error)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	TCP.ssl_socket.async_handshake(asio::ssl::stream_base::client,
	                               std::bind(&UWSClientSsl::ssl_handshake, this,
	                                         asio::placeholders::error));
}

void UWSClientSsl::ssl_handshake(const std::error_code& error)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	FString request;
	request = "GET " + RequestHandshake.Path + " HTTP/" + RequestHandshake.Version + "\r\n";
	if (!RequestHandshake.Headers.Contains("Host"))
	{
		request += "Host: " + Host + ":" + Service + "\r\n";
	}
	if (!RequestHandshake.Headers.Num() == 0)
	{
		for (const auto& header : RequestHandshake.Headers)
		{
			request += header.Key + ": " + header.Value + "\r\n";
		}
	}
	request += "\r\n";
	std::string payload = TCHAR_TO_UTF8(*request);
	asio::async_write(TCP.ssl_socket, asio::buffer(payload.c_str(), payload.length()),
	                  std::bind(&UWSClientSsl::write_handshake, this, asio::placeholders::error,
	                            asio::placeholders::bytes_transferred));
}

void UWSClientSsl::write_handshake(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_sent]()
	{
		OnBytesTransferred.Broadcast(bytes_sent, 0);
	});
	asio::async_read_until(TCP.ssl_socket, ResponseBuffer, "\r\n",
	                       std::bind(&UWSClientSsl::read_handshake, this, asio::placeholders::error,
	                                 asio::placeholders::bytes_transferred));
}

void UWSClientSsl::read_handshake(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&, bytes_recvd]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
	});
	UHttpFunctionLibrary::ClientClearResponse(ResponseHandshake);
	std::istream response_stream(&ResponseBuffer);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		ResponseHandshake.StatusCode = 505;
		ResponseHandshake.Body = "Invalid handshake: HTTP Version Not Supported.";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
			consume_response_buffer();
		});
		return;
	}
	ResponseHandshake.StatusCode = status_code;
	if (ResponseBuffer.size() == 0)
	{
		ResponseHandshake.Body = "Invalid handshake: " + ResponseStatusCode[status_code] + ".";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
			consume_response_buffer();
		});
		return;
	}
	asio::async_read_until(TCP.ssl_socket, ResponseBuffer, "\r\n\r\n",
	                       std::bind(&UWSClientSsl::read_headers, this, asio::placeholders::error));
}

void UWSClientSsl::read_headers(const std::error_code& error)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	UHttpFunctionLibrary::ClientClearResponse(ResponseHandshake);
	std::istream response_stream(&ResponseBuffer);
	std::string header;
	while (std::getline(response_stream, header) && header != "\r")
	{
		UHttpFunctionLibrary::ClientAppendHeader(ResponseHandshake, UTF8_TO_TCHAR(header.c_str()));
	}
	if (ResponseHandshake.Headers.Num() == 0) {
		ResponseHandshake.StatusCode = ResponseHandshake.StatusCode != 101 ? ResponseHandshake.StatusCode : 400;
		ResponseHandshake.Body = ResponseHandshake.StatusCode != 101
								 ? "Invalid handshake: " + ResponseStatusCode[ResponseHandshake.StatusCode]
								 : "Invalid handshake: Header is empty.";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
		});
		consume_response_buffer();
		return;
	}
	if (!ResponseHandshake.Headers.Contains("Connection") ||
		!ResponseHandshake.Headers.Contains("Upgrade") ||
		!ResponseHandshake.Headers.Contains("Sec-WebSocket-Accept")) {
		if (ResponseBuffer.size() > 0) {
			std::ostringstream body_buffer;
			body_buffer << &ResponseBuffer;
			UHttpFunctionLibrary::ClientSetBody(ResponseHandshake, UTF8_TO_TCHAR(body_buffer.str().c_str()));
		}
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
		});
		consume_response_buffer();
		return;
	}
	consume_response_buffer();
	if (ResponseHandshake.Headers["Connection"] != "Upgrade") {
		ResponseHandshake.Body = "Invalid handshake: \"Connection\" must be \"Upgrade\".";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
		});
		return;
	}
	if (ResponseHandshake.Headers["Upgrade"] != "websocket") {
		ResponseHandshake.Body = "Invalid handshake: \"Upgrade\" must be \"websocket\".";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
		});
		return;
	}
	FString protocol = RequestHandshake.Headers["Sec-WebSocket-Protocol"];
	FString res_protocol = ResponseHandshake.Headers["Sec-WebSocket-Protocol"];
	if (protocol.Find(res_protocol) == INDEX_NONE) {
		ResponseHandshake.Body = "Invalid handshake: \"Sec-WebSocket-Protocol\" must be \"" + protocol +
							 "\" or contain one of them.";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
		});
		return;
	}
	FString accept_key = ResponseHandshake.Headers["Sec-WebSocket-Accept"];
	FString encoded_key = generate_accept_key(RequestHandshake.Headers["Sec-WebSocket-Key"]);
	if (accept_key != encoded_key) {
		ResponseHandshake.Body = "Invalid handshake: \"Sec-WebSocket-Accept\" is invalid.";
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnHandshakeFail.Broadcast(ResponseHandshake);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		OnConnected.Broadcast(ResponseHandshake);
	});
	asio::async_read(
		TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
		std::bind(&UWSClientSsl::read, this, asio::placeholders::error,
		          asio::placeholders::bytes_transferred));
}

void UWSClientSsl::write(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{

			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
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

void UWSClientSsl::read(const std::error_code& error, const size_t bytes_recvd)
{
	if (error)
	{
		FScopeLock Guard(&MutexError);
		ErrorCode = error;
		AsyncTask(ENamedThreads::GameThread, [&, error]()
		{
			if (!error)
			{
				return;
			}
			ensureMsgf(!error, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), error.value(),
			           error.message().c_str());
			OnError.Broadcast(error);
		});
		return;
	}
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		OnBytesTransferred.Broadcast(0, bytes_recvd);
	});
	FWsMessage rDataFrame;
	if (!decode_payload(rDataFrame))
	{
		ResponseBuffer.consume(ResponseBuffer.size());
		asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
		                 std::bind(&UWSClientSsl::read, this, asio::placeholders::error,
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
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			OnPongReveived.Broadcast();
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
		AsyncTask(ENamedThreads::GameThread, [&, rDataFrame]()
		{
			OnMessageReceived.Broadcast(rDataFrame);
		});
	}
	consume_response_buffer();
	asio::async_read(TCP.ssl_socket, ResponseBuffer, asio::transfer_at_least(1),
	                 std::bind(&UWSClientSsl::read, this, asio::placeholders::error,
	                           asio::placeholders::bytes_transferred)
	);
}
