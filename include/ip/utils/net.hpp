#pragma once

#include "ip/net/common.hpp"
#include "buffer.hpp"

namespace internetprotocol {
    // HTTP
    std::string request_method_to_string(request_method_e method) {
        switch (method) {
            case CONNECT: return "CONNECT";
            case DEL: return "DELETE";
            case GET: return "GET";
            case HEAD: return "HEAD";
            case OPTIONS: return "OPTIONS";
            case PATCH: return "PATCH";
            case POST: return "POST";
            case PUT: return "PUT";
            case TRACE: return "TRACE";
            default:
                return "";
        }
    }

    request_method_e string_to_request_method(const std::string &method_str) {
        static const std::unordered_map<std::string, request_method_e> method_map = {
            {"CONNECT", CONNECT},
            {"DELETE", DEL},
            {"GET", GET},
            {"HEAD", HEAD},
            {"OPTIONS", OPTIONS},
            {"PATCH", PATCH},
            {"POST", POST},
            {"PUT", PUT},
            {"TRACE", TRACE}
        };

        auto it = method_map.find(method_str);
        if (it != method_map.end()) {
            return it->second;
        }

        return UNKNOWN;
    }

    inline std::string prepare_request(const http_request_t &req, const std::string &address, const uint16_t port) {
        std::string payload;
        payload.reserve(8192);

        payload = request_method_to_string(req.method) + " " + req.path;
        if (!req.params.empty()) {
            payload += "?";
            bool first = true;
            for (const std::pair<std::string, std::string> param: req.params) {
                if (!first) payload += "&";
                payload += param.first + "=" + param.second;
                first = false;
            }
        }
        payload += " HTTP/" + req.version + "\r\n";

        payload += "Host: " + address;
        if (port != 80 && port != 443) payload += ":" + std::to_string(port);
        payload += "\r\n";

        if (!req.headers.empty()) {
            for (const std::pair<std::string, std::string> header: req.headers) {
                payload += header.first + ": " + header.second + "\r\n";
            }
            if (req.headers.find("Content-Length") == req.headers.end() && req.body.length() > 0) {
                payload +=
                        "Content-Length: " + std::to_string(req.body.length()) + "\r\n";
            } else if (req.headers.find("content-length") == req.headers.end() && req.body.length() > 0) {
                payload +=
                        "content-length: " + std::to_string(req.body.length()) + "\r\n";
            }
        }
        payload += "\r\n";

        if (!req.body.empty()) payload += req.body;

        if (payload.capacity() > payload.length())
            payload.shrink_to_fit();

        return payload;
    }

    inline std::string prepare_response(const http_response_t &res) {
        std::string payload;

        payload = "HTTP/" + std::string(res.version) + " " + std::to_string(res.status_code) + " " + res.status_message
                  + "\r\n";
        if (!res.headers.empty()) {
            for (const std::pair<std::string, std::string> header: res.headers) {
                payload += header.first + ": " + header.second + "\r\n";
            }
            if (res.headers.find("Content-Length") == res.headers.end() && res.body.length() > 0) {
                payload +=
                        "Content-Length: " + std::to_string(res.body.length()) + "\r\n";
            } else if (res.headers.find("content-length") == res.headers.end() && res.body.length() > 0) {
                payload +=
                        "content-length: " + std::to_string(res.body.length()) + "\r\n";
            }
        }
        payload += "\r\n";
        if (!res.body.empty()) payload += res.body;

        return payload;
    }

    inline void req_append_header(http_request_t &req, const std::string &headerline) {
        size_t pos = headerline.find(':');
        if (pos != std::string::npos) {
            std::string key = trim_whitespace(headerline.substr(0, pos));
            string_to_lower(key);
            std::string value = trim_whitespace(headerline.substr(pos + 1));
            /*
            std::vector<std::string> values = split_string(value, ';');
            std::transform(
                values.begin(), values.end(), values.begin(),
                [&](const std::string &str) { return trim_whitespace(str); });
            */
            req.headers.insert_or_assign(key, value);
        }
    }

    inline void res_append_header(http_response_t &res, const std::string &headerline) {
        size_t pos = headerline.find(':');
        if (pos != std::string::npos) {
            std::string key = trim_whitespace(headerline.substr(0, pos));
            string_to_lower(key);
            std::string value = trim_whitespace(headerline.substr(pos + 1));
            /*
            std::vector<std::string> values = split_string(value, ';');
            std::transform(
                values.begin(), values.end(), values.begin(),
                [&](const std::string &str) { return trim_whitespace(str); });
            */
            res.headers.insert_or_assign(key, value);
        }
    }
}
