#pragma once

#include <string>
#include <vector>

namespace internetprotocol {
    inline std::string buffer_to_string(const std::vector<uint8_t> &buf) {
        std::string str;
        if (buf.empty()) return str;
        str.reserve(buf.size());
        str.assign(buf.begin(), buf.end());
        return str;
    }

    inline std::vector<std::string> split_string(const std::string &str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    inline std::string trim_whitespace(const std::string &str) {
        std::string::const_iterator start = str.begin();
        while (start != str.end() && std::isspace(*start)) {
            start++;
        }

        std::string::const_iterator end = str.end();
        do {
            end--;
        } while (std::distance(start, end) > 0 && std::isspace(*end));

        return std::string(start, end + 1);
    }

    inline void string_to_lower(std::string &str) {
        std::string lower_str;
        lower_str.reserve(str.size());
        for (char &c : str) {
            c = static_cast<char>(std::tolower(c));
        }
    }
}