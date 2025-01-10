/*
 * Copyright (c) 2023-2024 Nathan Miguel
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

#pragma once

#include <IP/Net/Common.hpp>

namespace InternetProtocol {
    struct FUdpMessage {
        FUdpMessage() { raw_data.resize(1024); }

        ~FUdpMessage() {
            raw_data.clear();
            raw_data.shrink_to_fit();
        }

        std::vector<uint8_t> raw_data;
        size_t size = 0;

        std::string toString() const {
            std::string str;
            str.resize(size);
            std::transform(raw_data.begin(), raw_data.end(), str.begin(),
                           [](uint8_t byte) { return static_cast<char>(byte); });
            return str;
        }
    };

    struct FTcpMessage {
        ~FTcpMessage() {
            raw_data.clear();
            raw_data.shrink_to_fit();
        }

        std::vector<uint8_t> raw_data;
        size_t size = 0;
    };

    struct FWsMessage {
        ~FWsMessage() {
            payload.clear();
            payload.shrink_to_fit();
        }

        FDataFrame data_frame;
        std::vector<uint8_t> payload;
        size_t size = 0;
    };
} // InternetProtocol
