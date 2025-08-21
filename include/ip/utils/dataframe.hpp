/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
*/

#pragma once

#include <random>

namespace internetprotocol {
    inline std::array<uint8_t, 4> mask_gen() {
        std::array<uint8_t, 4> maskKey{};
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        for (uint8_t &byte: maskKey) {
            byte = uint8_t(dis(gen));
        }

        return maskKey;
    }

    inline std::string encode_string_payload(const std::string &payload, const dataframe_t &dataframe) {
        std::string string_buffer;

        uint64_t payload_length = payload.size();
        uint64_t header_size = 2;
        if (payload_length > 125 && payload_length <= 65535) {
            header_size += 2;
        } else if (payload_length > 65535) {
            header_size += 8;
        }
        if (dataframe.mask) {
            header_size += 4;
        }

        string_buffer.reserve(header_size + payload_length);

        // FIN, RSV, Opcode
        uint8_t byte1 = dataframe.fin ? 0x80 : 0x00;
        byte1 |= dataframe.rsv1 ? (uint8_t) RSV1 : 0x00;
        byte1 |= dataframe.rsv2 ? (uint8_t) RSV2 : 0x00;
        byte1 |= dataframe.rsv3 ? (uint8_t) RSV3 : 0x00;
        byte1 |= (uint8_t) dataframe.opcode & 0x0F;
        string_buffer.push_back(byte1);

        // Mask and payload size
        uint8_t byte2 = dataframe.mask ? 0x80 : 0x00;
        if (payload_length <= 125) {
            byte2 |= payload_length;
            string_buffer.push_back(byte2);
        } else if (payload_length <= 65535) {
            byte2 |= 126;
            string_buffer.push_back(byte2);
            string_buffer.push_back((payload_length >> 8) & 0xFF);
            string_buffer.push_back(payload_length & 0xFF);
        } else {
            byte2 |= 127;
            string_buffer.push_back(byte2);
            for (int i = 7; i >= 0; --i) {
                string_buffer.push_back((payload_length >> (8 * i)) & 0xFF);
            }
        }

        std::array<uint8_t, 4> masking_key{};
        if (dataframe.mask) {
            masking_key = mask_gen();
            for (uint8_t key: masking_key)
                string_buffer.push_back(key);
        }

        // payload data and mask
        for (size_t i = 0; i < payload.size(); ++i) {
            if (dataframe.mask) {
                string_buffer.push_back(payload[i] ^ masking_key[i % 4]);
            } else {
                string_buffer.push_back(payload[i]);
            }
        }

        if (string_buffer.capacity() > string_buffer.size())
            string_buffer.shrink_to_fit();

        return string_buffer;
    }

    inline std::vector<uint8_t>
    encode_buffer_payload(const std::vector<uint8_t> &payload, const dataframe_t &dataframe) {
        std::vector<uint8_t> buffer;

        uint64_t payload_length = payload.size();
        uint64_t header_size = 2;
        if (payload_length > 125 && payload_length <= 65535) {
            header_size += 2;
        } else if (payload_length > 65535) {
            header_size += 8;
        }
        if (dataframe.mask) {
            header_size += 4;
        }

        buffer.reserve(header_size + payload_length);

        // FIN, RSV, Opcode
        uint8_t byte1 = uint8_t(dataframe.fin ? 0x80 : 0x00);
        byte1 |= uint8_t(dataframe.rsv1 ? (uint8_t) RSV1 : 0x00);
        byte1 |= uint8_t(dataframe.rsv2 ? (uint8_t) RSV2 : 0x00);
        byte1 |= uint8_t(dataframe.rsv3 ? (uint8_t) RSV3 : 0x00);
        byte1 |= uint8_t((uint8_t) dataframe.opcode & 0x0F);
        buffer.push_back(byte1);

        // Mask and payload size
        uint8_t byte2 = uint8_t(dataframe.mask ? 0x80 : 0x00);
        if (payload_length <= 125) {
            byte2 |= uint8_t(payload_length);
            buffer.push_back(byte2);
        } else if (payload_length <= 65535) {
            byte2 |= uint8_t(126);
            buffer.push_back(byte2);
            buffer.push_back(uint8_t((payload_length >> 8) & 0xFF));
            buffer.push_back(uint8_t(payload_length & 0xFF));
        } else {
            byte2 |= uint8_t(127);
            buffer.push_back(byte2);
            for (int i = 7; i >= 0; --i) {
                buffer.push_back(uint8_t((payload_length >> (8 * i)) & 0xFF));
            }
        }

        std::array<uint8_t, 4> masking_key{};
        if (dataframe.mask) {
            masking_key = mask_gen();
            for (uint8_t key: masking_key) {
                buffer.push_back(key);
            }
        }

        if (!payload.empty()) {
            // payload data and mask
            for (size_t i = 0; i < payload.size(); ++i) {
                if (dataframe.mask) {
                    buffer.push_back(payload[i] ^ masking_key[i % 4]);
                } else {
                    buffer.push_back(payload[i]);
                }
            }
        }

        if (buffer.capacity() > buffer.size())
            buffer.shrink_to_fit();

        return buffer;
    }

    inline bool decode_payload(const std::vector<uint8_t> &buffer, std::vector<uint8_t> &payload,
                               dataframe_t &dataframe) {
        size_t pos = 0;
        if (buffer.size() < 2) return false;

        // FIN, RSV, Opcode
        const uint8_t byte1 = buffer[pos++];
        dataframe.fin = byte1 & 0x80;
        dataframe.rsv1 = byte1 & 0x40;
        dataframe.rsv2 = byte1 & 0x20;
        dataframe.rsv3 = byte1 & 0x10;
        dataframe.opcode = static_cast<opcode_e>(byte1 & 0x0F);

        // Mask and payload length
        uint8_t byte2 = buffer[pos++];
        dataframe.mask = byte2 & 0x80;
        uint64_t payload_length = byte2 & 0x7F;

        if (payload_length == 126) {
            if (buffer.size() < pos + 2) return false;
            payload_length = (static_cast<uint64_t>(buffer[pos]) << 8) | buffer[pos + 1];
            pos += 2;
        } else if (payload_length == 127) {
            if (buffer.size() < pos + 8) return false;
            payload_length = 0;
            for (int i = 0; i < 8; ++i) {
                payload_length = (payload_length << 8) | buffer[pos + i];
            }
            pos += 8;
        }
        dataframe.length = payload_length;

        // Masking key
        if (dataframe.mask) {
            if (buffer.size() < pos + 4) return false;
            for (int i = 0; i < 4; ++i) {
                dataframe.masking_key[i] = buffer[pos++];
            }
        }

        // Payload data
        if (buffer.size() < pos + payload_length) return false;
        payload.clear();
        payload.reserve(payload_length);

        for (size_t i = 0; i < payload_length; ++i) {
            uint8_t byte = buffer[pos++];
            if (dataframe.mask) {
                byte ^= dataframe.masking_key[i % 4];
            }
            payload.push_back(byte);
        }

        return true;
    }
}
