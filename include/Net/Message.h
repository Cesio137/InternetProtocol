#pragma once
#include "Net/Common.h"

namespace InternetProtocol {
	struct udpMessage {
		std::array<char, 1024> message;
		std::string toString() const {
			return message.data();
		}
		uint32_t size = 0;
	};
} //InternetProtocol