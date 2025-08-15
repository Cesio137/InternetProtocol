/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
*/

#pragma once

#include <asio.hpp>

namespace internetprotocol {
    std::vector<asio::const_buffer> slice_string(const std::string &str, const size_t size) {
        std::vector<asio::const_buffer> buffers;
        // Return only one aso buffer if str->size() <= size
        if (str.size() <= size) {
            buffers.reserve(1);
            buffers.push_back(asio::buffer(str.data(), str.size()));
            return buffers;
        }

        // Cache the buffers size
        const size_t packs_len = str.size() / size;
        buffers.reserve((str.size() % size == 0) ? packs_len : packs_len + 1);

        // Create another cache to split str
        size_t string_offset = 0;
        std::string str_splited;
        str_splited.reserve(size);
        while (string_offset < str.size()) {
            size_t pack_size = std::min(size, str.size() - string_offset);
            str_splited.assign(str.begin() + string_offset,
                                    str.begin() + string_offset + pack_size);
            buffers.push_back(asio::buffer(str_splited.data(), str_splited.size()));
            str_splited.clear();
            string_offset += pack_size;
        }

        return buffers;
    }

    std::vector<asio::const_buffer> slice_buffer(const std::vector<uint8_t> &buf, const size_t size) {
        std::vector<asio::const_buffer> buffers;

        // Return only one aso buffer if buf->size() <= size
        if (buf.size() <= size) {
            buffers.reserve(1);
            buffers.push_back(asio::buffer(buf.data(), buf.size()));
            return buffers;
        }

        // Cache the buffers size
        const size_t packs_len = buf.size() / size;
        buffers.reserve((buf.size() % size == 0) ? packs_len : packs_len + 1);

        // Create another cache to split buffer
        size_t buffer_offset = 0;
        std::vector<uint8_t> buffer_splited;
        buffer_splited.reserve(size);
        while (buffer_offset < size) {
            size_t package_size = std::min(size, buf.size() - buffer_offset);
            buffer_splited.assign(buf.begin() + buffer_offset,
                                            buf.begin() + buffer_offset + package_size);
            buffers.push_back(asio::buffer(buffer_splited.data(), buffer_splited.size()));
            buffer_splited.clear();
            buffer_offset += package_size;
        }

        return buffers;
    }
}
