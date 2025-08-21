/** 
 * Copyright © 2025 Nathan Miguel
 */

#include "utils/dataframe.h"

std::array<uint8, 4> mask_gen() {
	std::array<uint8, 4> maskKey{};
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 255);

	for (uint8 &byte: maskKey) {
		byte = uint8(dis(gen));
	}

	return maskKey;
}

FString encode_string_payload(const FString& payload, const FDataframe& dataframe) {
	FString string_buffer;
	
	uint64 payload_length = payload.Len();
	uint64 header_size = 2;
	if (payload_length > 125 && payload_length <= 65535) {
		header_size += 2;
	} else if (payload_length > 65535) {
		header_size += 8;
	}
	if (dataframe.bMask) {
		header_size += 4;
	}

	string_buffer.Reserve(header_size + payload_length);

	// FIN, RSV, Opcode
	uint8 byte1 = dataframe.bFin ? 0x80 : 0x00;
	byte1 |= dataframe.bRsv1 ? (uint8) ERSV::RSV1 : 0x00;
	byte1 |= dataframe.bRsv2 ? (uint8) ERSV::RSV2 : 0x00;
	byte1 |= dataframe.bRsv3 ? (uint8) ERSV::RSV3 : 0x00;
	byte1 |= (uint8) dataframe.Opcode & 0x0F;
	string_buffer.AppendChar(byte1);

	// Mask and payload size
	uint8 byte2 = dataframe.bMask ? 0x80 : 0x00;
	if (payload_length <= 125) {
		byte2 |= payload_length;
		string_buffer.AppendChar(byte2);
	} else if (payload_length <= 65535) {
		byte2 |= 126;
		string_buffer.AppendChar(byte2);
		string_buffer.AppendChar((payload_length >> 8) & 0xFF);
		string_buffer.AppendChar(payload_length & 0xFF);
	} else {
		byte2 |= 127;
		string_buffer.AppendChar(byte2);
		for (int i = 7; i >= 0; --i) {
			string_buffer.AppendChar((payload_length >> (8 * i)) & 0xFF);
		}
	}

	std::array<uint8, 4> masking_key{};
	if (dataframe.bMask) {
		masking_key = mask_gen();
		for (uint8 key: masking_key)
			string_buffer.AppendChar(key);
	}

	// payload data and mask
	for (size_t i = 0; i < payload.Len(); ++i) {
		if (dataframe.bMask) {
			string_buffer.AppendChar(payload[i] ^ masking_key[i % 4]);
		} else {
			string_buffer.AppendChar(payload[i]);
		}
	}

	if (string_buffer.GetAllocatedSize() > string_buffer.Len())
		string_buffer.Shrink();

	return string_buffer;
}

bool decode_payload(const TArray<uint8>& buffer, TArray<uint8>& payload, FDataframe& dataframe) {
	size_t pos = 0;
	if (buffer.Num() < 2) return false;

	// FIN, RSV, Opcode
	const uint8 byte1 = buffer[pos++];
	dataframe.bFin = byte1 & 0x80;
	dataframe.bRsv1 = byte1 & 0x40;
	dataframe.bRsv2 = byte1 & 0x20;
	dataframe.bRsv3 = byte1 & 0x10;
	dataframe.Opcode = static_cast<EOpcode>(byte1 & 0x0F);

	// Mask and payload length
	uint8 byte2 = buffer[pos++];
	dataframe.bMask = byte2 & 0x80;
	uint64 payload_length = byte2 & 0x7F;

	if (payload_length == 126) {
		if (buffer.Num() < pos + 2) return false;
		payload_length = (static_cast<uint64>(buffer[pos]) << 8) | buffer[pos + 1];
		pos += 2;
	} else if (payload_length == 127) {
		if (buffer.Num() < pos + 8) return false;
		payload_length = 0;
		for (int i = 0; i < 8; ++i) {
			payload_length = (payload_length << 8) | buffer[pos + i];
		}
		pos += 8;
	}
	dataframe.Length = payload_length;

	// Masking key
	if (dataframe.bMask) {
		if (buffer.Num() < pos + 4) return false;
		for (int i = 0; i < 4; ++i) {
			dataframe.masking_key[i] = buffer[pos++];
		}
	}

	// Payload data
	if (buffer.Num() < pos + payload_length) return false;
	payload.Empty();
	payload.Reserve(payload_length);

	for (size_t i = 0; i < payload_length; ++i) {
		uint8_t byte = buffer[pos++];
		if (dataframe.bMask) {
			byte ^= dataframe.masking_key[i % 4];
		}
		payload.Add(byte);
	}

	return true;
}

TArray<uint8> encode_buffer_payload(const TArray<uint8>& payload, const FDataframe& dataframe) {
	TArray<uint8> buffer;
	
	uint64 payload_length = payload.Num();
	uint64 header_size = 2;
	if (payload_length > 125 && payload_length <= 65535) {
		header_size += 2;
	} else if (payload_length > 65535) {
		header_size += 8;
	}
	if (dataframe.bMask) {
		header_size += 4;
	}

	buffer.Reserve(header_size + payload_length);

	// FIN, RSV, Opcode
	uint8_t byte1 = uint8(dataframe.bFin ? 0x80 : 0x00);
	byte1 |= uint8(dataframe.bRsv1 ? (uint8) ERSV::RSV1 : 0x00);
	byte1 |= uint8(dataframe.bRsv2 ? (uint8) ERSV::RSV2 : 0x00);
	byte1 |= uint8(dataframe.bRsv3 ? (uint8) ERSV::RSV3 : 0x00);
	byte1 |= uint8((uint8_t) dataframe.Opcode & 0x0F);
	buffer.Add(byte1);

	// Mask and payload size
	uint8 byte2 = uint8(dataframe.bMask ? 0x80 : 0x00);
	if (payload_length <= 125) {
		byte2 |= uint8(payload_length);
		buffer.Add(byte2);
	} else if (payload_length <= 65535) {
		byte2 |= uint8(126);
		buffer.Add(byte2);
		buffer.Add(uint8((payload_length >> 8) & 0xFF));
		buffer.Add(uint8(payload_length & 0xFF));
	} else {
		byte2 |= uint8(127);
		buffer.Add(byte2);
		for (int i = 7; i >= 0; --i) {
			buffer.Add(uint8((payload_length >> (8 * i)) & 0xFF));
		}
	}

	std::array<uint8, 4> masking_key{};
	if (dataframe.bMask) {
		masking_key = mask_gen();
		for (uint8 key: masking_key) {
			buffer.Add(key);
		}
	}

	if (payload.Num()) {
		// payload data and mask
		for (size_t i = 0; i < payload.Num(); ++i) {
			if (dataframe.bMask) {
				buffer.Add(payload[i] ^ masking_key[i % 4]);
			} else {
				buffer.Add(payload[i]);
			}
		}
	}

	if (buffer.GetAllocatedSize() > buffer.Num())
		buffer.Shrink();

	return buffer;
}
