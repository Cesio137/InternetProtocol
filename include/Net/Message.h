#pragma once

#include "Net/Common.h"

namespace InternetProtocol {
	struct FTcpMessage {
		std::array<uint8_t, 8192> rawData;
		std::string toUTF8() const {
			std::string str(rawData.begin(), rawData.begin() + size);
			return str;
		}
		uint32_t size = 0;
	};

	struct FDataFrame {
		bool fin = true;
		bool rsv1 = false;
		bool rsv2 = false;
		bool rsv3 = false;
		bool mask = true;
		EOpcode opcode = EOpcode::TEXT_FRAME;
		std::vector<uint8_t> payload;
	};

	struct FUdpMessage {
		std::array<uint8_t, 1024> rawData;
		std::string toUTF8() const {
			std::string str(rawData.begin(), rawData.begin() + size);
			return str;
		}
		uint32_t size = 0;
	};
} //InternetProtocol