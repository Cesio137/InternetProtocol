#pragma once

#include "Net/Common.h"

namespace InternetProtocol {
    struct FUdpMessage {
        FUdpMessage() { rawData.resize(1024); }
        std::vector<std::byte> rawData;

        std::string toUTF8() const {
            std::string str;
            str.resize(size);
            std::transform(rawData.begin(), rawData.end(), str.begin(),
                           [](std::byte byte) { return static_cast<char>(byte); });
            return str;
        }

        size_t size = 0;
    };

    struct FTcpMessage {
        std::vector<std::byte> raw_data;

        std::string toUTF8() const {
            std::string str;
            str.resize(size);
            std::transform(raw_data.begin(), raw_data.end(), str.begin(),
                           [](std::byte byte) { return static_cast<char>(byte); });
            return str;
        }

        uint32_t size = 0;
    };

    struct FWsMessage {
        FDataFrame data_frame;
        std::vector<std::byte> payload;

        std::string toUTF8() const {
            if (data_frame.opcode != EOpcode::TEXT_FRAME || data_frame.opcode != EOpcode::BINARY_FRAME) {
                std::string str;
                str.resize(payload.size());
                std::transform(payload.begin(), payload.end(), str.begin(),
                               [](std::byte byte) { return static_cast<char>(byte); });
                return str;
            }
            return "";
        }
    };
} // InternetProtocol
