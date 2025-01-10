#pragma once

#include <IP/Net/Common.hpp>

namespace InternetProtocol {
    /*MESSAGE*/
    inline std::string buffer_to_string(const std::vector<uint8_t> &buffer) {
        std::string str;
        str.resize(buffer.size());
        std::transform(buffer.begin(), buffer.end(), str.begin(),
                       [&](uint8_t byte) { return static_cast<char>(byte); });
        return str;
    }

    /*HTTP REQUEST*/
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

    namespace Server {
        inline void req_append_header(FRequest &req, const std::string &headerline) {
            size_t pos = headerline.find(':');
            if (pos != std::string::npos) {
                std::string key = trim_whitespace(headerline.substr(0, pos));
                std::string value = trim_whitespace(headerline.substr(pos + 1));
                std::vector<std::string> values = split_string(value, ';');
                std::transform(
                    values.begin(), values.end(), values.begin(),
                    [&](const std::string &str) { return trim_whitespace(str); });
                req.headers.insert_or_assign(key, values);
            }
        }

        inline void req_set_body(FRequest &req, const std::string &value) {
            if (value.empty()) return;
            req.body = value;
        }

        inline void req_append_body(FRequest &res, const std::string &value) {
            if (value.empty()) return;
            res.body.append(value);
        }
    }

    namespace Client {
        inline void res_append_header(Client::FResponse &res, const std::string &headerline) {
            size_t pos = headerline.find(':');
            if (pos != std::string::npos) {
                std::string key = trim_whitespace(headerline.substr(0, pos));
                std::string value = trim_whitespace(headerline.substr(pos + 1));
                if (key == "Content-Length") {
                    res.content_lenght = std::stoi(value);
                    return;
                }
                std::vector<std::string> values = split_string(value, ';');
                std::transform(
                    values.begin(), values.end(), values.begin(),
                    [&](const std::string &str) { return trim_whitespace(str); });
                res.headers.insert_or_assign(key, values);
            }
        }

        inline void res_set_body(FResponse &res, const std::string &value) {
            if (value.empty()) return;
            res.body = value;
        }

        inline void res_append_body(FResponse &res, const std::string &value) {
            if (value.empty()) return;
            res.body.append(value);
        }

        inline void res_clear(FResponse &res) {
            if (!res.headers.empty())
                res.headers.clear();
            res.content_lenght = 0;
            res.body.clear();
        }

        inline void req_clear(FRequest &req) {
            req.params.clear();
            req.method = EMethod::GET;
            req.path = "/";
            req.version = "1.1";
            req.headers.clear();
            req.body.clear();
        }
    }
}
