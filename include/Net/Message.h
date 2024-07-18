#pragma once

#include "Net/Common.h"

namespace InternetProtocol {
	struct tcpMessage {
		std::array<char, 8192> message;
		std::string toString() const {
			return message.data();
		}
		uint32_t size = 0;
	};

	struct clientDataFrame {
		bool fin = true;
		bool rsv1 = false;
		bool rsv2 = false;
		bool rsv3 = false;
		bool mask = true;
		EOpcode opcode = EOpcode::TEXT_FRAME;
		std::vector<uint8_t> payload;
		
		std::vector<uint8_t> encodeFrame() {
			std::vector<uint8_t> buffer;
			uint64_t payload_length = payload.size();

			// FIN, RSV, Opcode
			uint8_t byte1 = fin ? 0x80 : 0x00;
			if(rsv1) byte1 |= (uint8_t)ERSV::RSV1;
			if(rsv2) byte1 |= (uint8_t)ERSV::RSV2;
			if(rsv3) byte1 |= (uint8_t)ERSV::RSV3;
			byte1 |= ((uint8_t)opcode & 0x0F);
			buffer.push_back(byte1);

			// Mask and payload size
			uint8_t byte2 = mask ? 0x80 : 0x00;
			if (payload_length <= 125) {
				byte2 |= payload_length;
				buffer.push_back(byte2);
			} else if (payload_length <= 65535) {
				byte2 |= 126;
				buffer.push_back(byte2);
				buffer.push_back((payload_length >> 8) & 0xFF);
				buffer.push_back(payload_length & 0xFF);
			} else {
				byte2 |= 127;
				buffer.push_back(byte2);
				for (int i = 7; i >= 0; --i) {
					buffer.push_back((payload_length >> (8 * i)) & 0xFF);
				}
			}

			std::array<uint8_t, 4> masking_key;
			if (mask) {
				for (uint8_t key : masking_key) buffer.push_back(key);
			}

			// payload data
			for (uint8_t data : payload) {
				buffer.push_back(data);
			}

			// payload mask
			for (size_t i = 0; i < payload.size(); ++i) {
				if (mask) {
					buffer.push_back(payload[i] ^ masking_key[i % 4]);
				} else {
					buffer.push_back(payload[i]);
				}
			}

			return buffer;
		}

	private:
		std::array<uint8_t, 4> mask_gen() {
			std::array<uint8_t, 4> maskKey;
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<> dis(1, 255);

			for (uint8_t& byte : maskKey) {
				byte = (uint8_t)dis(gen);
			}

			return maskKey;
		}
	};
	/*
	struct wsFrameMessage {
		bool fin = true;  
    	uint8_t opcode = static_cast<uint8_t>(EOptcode::TEXT_FRAME);
    	bool mask = true;
    	std::vector<uint8_t> payload;

		std::vector<uint8_t> encodeFrame() {
			std::vector<uint8_t> buffer;
			uint64_t payload_length = payload.size();

			// FIN, RSV, Opcode
			uint8_t byte1 = (fin ? 0x80 : 0x00) | (opcode & 0x0F);
			buffer.push_back(byte1);

			// mask and payload
			masking_gen();
			uint8_t byte2 = mask ? 0x80 : 0x00;
			if (payload_length <= 125) {
				byte2 |= payload_length;
				buffer.push_back(byte2);
			} 
			else if (payload_length <= 65535) {
				byte2 |= 126;
				buffer.push_back(byte2);
				buffer.push_back((payload_length >> 8) & 0xFF);
				buffer.push_back(payload_length & 0xFF);
			} 
			else {
				byte2 |= 127;
				buffer.push_back(byte2);
				for (int i = 7; i >= 0; --i) {
					buffer.push_back((payload_length >> (8 * i)) & 0xFF);
				}
			}

			// payload data
			for (size_t i = 0; i < payload.size(); ++i) {
				buffer.push_back(payload[i]);
			}

			// payload mask
			if (mask) {
				
				for (int i = 0; i < 4; ++i) {
					buffer.push_back(masking_key[i]);
				}
				
				for (size_t i = 0; i < buffer.size(); ++i) {
					buffer[i] ^= masking_key[i % 4];
				}
			}
			

			return buffer;
		}
		
		void decodeFrame(const std::vector<uint8_t>& buffer) {
			size_t pos = 0;

			// Primeira parte: FIN, RSV, Opcode
			uint8_t byte1 = buffer[pos++];
			fin = (byte1 & 0x80) != 0;
			opcode = byte1 & 0x0F;

			// Segunda parte: Máscara e comprimento da carga útil
			uint8_t byte2 = buffer[pos++];
			mask = (byte2 & 0x80) != 0;
			uint64_t payload_length = byte2 & 0x7F;

			if (payload_length == 126) {
				payload_length = (buffer[pos++] << 8) | buffer[pos++];
			} else if (payload_length == 127) {
				payload_length = 0;
				for (int i = 0; i < 8; ++i) {
					payload_length = (payload_length << 8) | buffer[pos++];
				}
			}

			// Máscara de carga útil, se existir
			if (mask) {
				for (int i = 0; i < 4; ++i) {
					masking_key[i] = buffer[pos++];
				}
			}

			// Dados da carga útil
			payload.resize(payload_length);
			for (size_t i = 0; i < payload_length; ++i) {
				payload[i] = buffer[pos++];
			}
		}

	private:
		uint8_t masking_key[4];
		void masking_gen() {
			// gen random values for mask
			std::srand(std::time(0));
			for (int i = 0; i < 4; ++i) {
				masking_key[i] = std::rand() % 255;
			}
		}
	};
	*/

	struct udpMessage {
		std::array<char, 1024> message;
		std::string toString() const {
			return message.data();
		}
		uint32_t size = 0;
	};
} //InternetProtocol