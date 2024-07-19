#pragma once

#include "Net/Common.h"

namespace InternetProtocol {
	struct FTcpMessage {
		std::array<char, 8192> message;
		std::string toString() const {
			return message.data();
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
		std::array<char, 1024> message;
		std::string toString() const {
			return message.data();
		}
		uint32_t size = 0;
	};
} //InternetProtocol