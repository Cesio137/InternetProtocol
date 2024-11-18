#pragma once

#include <IP/Net/Common.hpp>

namespace InternetProtocol {
    struct FUdpMessage {
        FUdpMessage() { raw_data.resize(1024); }
        ~FUdpMessage() { 
            raw_data.clear();
            raw_data.shrink_to_fit();
        }
        std::vector<std::byte> raw_data;
        size_t size = 0;

        std::string toString() const {
            std::string str;
            str.resize(size);
            std::transform(raw_data.begin(), raw_data.end(), str.begin(),
                           [](std::byte byte) { return static_cast<char>(byte); });
            return str;
        }
    };

    struct FTcpMessage {
        ~FTcpMessage() { 
            raw_data.clear();
            raw_data.shrink_to_fit();
        }
        std::vector<std::byte> raw_data;
        uint32_t size = 0;

        std::string toString() const {
            std::string str;
            str.resize(size);
            std::transform(raw_data.begin(), raw_data.end(), str.begin(),
                           [](std::byte byte) { return static_cast<char>(byte); });
            return str;
        }
    };

    struct FWsMessage {
        ~FWsMessage() {
            payload.clear();
            payload.shrink_to_fit();
        }
        FDataFrame data_frame;
        std::vector<std::byte> payload;

        std::string toString() const {
            if (data_frame.opcode == EOpcode::TEXT_FRAME || data_frame.opcode == EOpcode::BINARY_FRAME) {
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
