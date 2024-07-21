#pragma once

#include "Net/Common.h"

namespace InternetProtocol {
	struct FUdpMessage {
		FUdpMessage() { rawData.resize(1024); }
		std::vector<uint8_t> rawData;
		std::string toUTF8() const {
			return std::string(rawData.begin(), rawData.end());
		}
		uint32_t size = 0;
	};

	struct FTcpMessage {
		std::vector<uint8_t> rawData;
		std::string toUTF8() const {
			return std::string(rawData.begin(), rawData.end());
		}
		uint32_t size = 0;
	};

	struct FWsMessage {
		FDataFrame dataFrame;
		std::vector<uint8_t> payload;
		std::string toUTF8() const {
			return std::string(payload.begin(), payload.end());		
		}
	};
} //InternetProtocol