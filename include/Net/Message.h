#pragma once

#include "Net/Common.h"

namespace InternetProtocol {
	struct FTcpMessage {
		FTcpMessage() { rawData.resize(8192); }
		std::vector<uint8_t> rawData;
		std::string toUTF8() const {
			return std::string(rawData.begin(), rawData.end());
		}
		uint32_t size = 0;
	};

	struct FDataFrame {
		FDataFrame() { payload.resize(8192); }
		bool fin = true;
		bool rsv1 = false;
		bool rsv2 = false;
		bool rsv3 = false;
		bool mask = true;
		EOpcode opcode = EOpcode::TEXT_FRAME;
		std::vector<uint8_t> payload;
		size_t length = 0;
		std::array<uint8_t, 4> masking_key;
		std::string toUTF8() const {
			if (opcode == EOpcode::TEXT_FRAME)
				return std::string(payload.begin(), payload.end());
			return "";		
		}
	};

	struct FUdpMessage {
		FUdpMessage() { rawData.resize(1024); }
		std::vector<uint8_t> rawData;
		std::string toUTF8() const {
			return std::string(rawData.begin(), rawData.end());
		}
		uint32_t size = 0;
	};
} //InternetProtocol