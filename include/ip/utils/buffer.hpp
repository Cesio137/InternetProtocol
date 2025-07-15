#pragma once

#include <string>
#include <vector>

namespace ip {
    /*MESSAGE*/
    inline std::string buffer_to_string(const std::vector<uint8_t> &buf) {
        std::string str;
        if (buf.empty()) return str;
        str.reserve(buf.size());
        str.assign(buf.begin(), buf.end());
        return str;
    }
}