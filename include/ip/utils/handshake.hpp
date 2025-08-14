#pragma once

#include <string>
#include <vector>

namespace internetprotocol {
    // SHA1
    static uint32_t h0 = 0x67452301;
    static uint32_t h1 = 0xEFCDAB89;
    static uint32_t h2 = 0x98BADCFE;
    static uint32_t h3 = 0x10325476;
    static uint32_t h4 = 0xC3D2E1F0;

    // Accept key
    static std::string magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    // Base encoded
    inline const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    inline std::array<uint8_t, 20> sha1(const std::string &input) {
        uint64_t bit_length = input.size() * 8;
        std::string padded_input = input;

        padded_input += static_cast<char>(0x80);

        while ((padded_input.size() % 64) != 56) {
            padded_input += static_cast<char>(0x00);
        }

        for (int i = 7; i >= 0; --i) {
            padded_input += static_cast<char>((bit_length >> (i * 8)) & 0xFF);
        }

        uint32_t h[5] = {h0, h1, h2, h3, h4};

        for (size_t i = 0; i < padded_input.size(); i += 64) {
            uint32_t w[80] = {0};

            for (int j = 0; j < 16; ++j) {
                w[j] = static_cast<uint8_t>(padded_input[i + j * 4]) << 24 |
                       static_cast<uint8_t>(padded_input[i + j * 4 + 1]) << 16 |
                       static_cast<uint8_t>(padded_input[i + j * 4 + 2]) << 8 |
                       static_cast<uint8_t>(padded_input[i + j * 4 + 3]);
            }

            for (int j = 16; j < 80; ++j) {
                w[j] = (w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16]);
                w[j] = (w[j] << 1) | (w[j] >> 31);
            }

            uint32_t a = h[0];
            uint32_t b = h[1];
            uint32_t c = h[2];
            uint32_t d = h[3];
            uint32_t e = h[4];

            for (int j = 0; j < 80; ++j) {
                uint32_t f, k;

                if (j < 20) {
                    f = (b & c) | ((~b) & d);
                    k = 0x5A827999;
                } else if (j < 40) {
                    f = b ^ c ^ d;
                    k = 0x6ED9EBA1;
                } else if (j < 60) {
                    f = (b & c) | (b & d) | (c & d);
                    k = 0x8F1BBCDC;
                } else {
                    f = b ^ c ^ d;
                    k = 0xCA62C1D6;
                }

                uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[j];
                e = d;
                d = c;
                c = (b << 30) | (b >> 2);
                b = a;
                a = temp;
            }

            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
            h[4] += e;
        }

        std::array<uint8_t, 20> hash{};
        for (int i = 0; i < 5; ++i) {
            hash[i * 4] = (h[i] >> 24) & 0xFF;
            hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
            hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
            hash[i * 4 + 3] = h[i] & 0xFF;
        }

        return hash;
    }

    inline std::string base64_encode(const uint8_t *input, size_t length) {
        std::string result;
        int i = 0;
        uint32_t octet_a, octet_b, octet_c, triple;

        while (length >= 3) {
            octet_a = input[i++];
            octet_b = input[i++];
            octet_c = input[i++];

            triple = (octet_a << 16) + (octet_b << 8) + octet_c;

            result.push_back(base64_chars[(triple >> 18) & 0x3F]);
            result.push_back(base64_chars[(triple >> 12) & 0x3F]);
            result.push_back(base64_chars[(triple >> 6) & 0x3F]);
            result.push_back(base64_chars[triple & 0x3F]);

            length -= 3;
        }

        if (length > 0) {
            octet_a = input[i++];
            octet_b = length > 1 ? input[i++] : 0;

            triple = (octet_a << 16) + (octet_b << 8);

            result.push_back(base64_chars[(triple >> 18) & 0x3F]);
            result.push_back(base64_chars[(triple >> 12) & 0x3F]);
            result.push_back(length == 2 ? base64_chars[(triple >> 6) & 0x3F] : '=');
            result.push_back('=');
        }

        return result;
    }

    inline std::string generate_accept_key(const std::string &sec_websocket_key) {
        std::string concatenated = sec_websocket_key + magic_string;
        auto hash = sha1(concatenated);
        return base64_encode(hash.data(), hash.size());
    }

    inline bool validate_handshake_request(const http_request_t &req_handshake, http_response_t &res_handshake) {
        if (req_handshake.headers.find("connection") == req_handshake.headers.end()) {
            res_handshake.body = "\"Connection\" header not found";
            return false;
        }
        if (req_handshake.headers.at("connection") != "Upgrade") {
            res_handshake.body = "\"Connection\" header value is not \"Upgrade\"";
            return false;
        }

        if (req_handshake.headers.find("upgrade") == req_handshake.headers.end()) {
            res_handshake.body = "\"Upgrade\" header not found";
            return false;
        }
        if (req_handshake.headers.at("upgrade") != "websocket") {
            res_handshake.body = "\"Upgrade\" header value is not \"websocket\"";
            return false;
        }

        if (req_handshake.headers.find("sec-websocket-key") == req_handshake.headers.end()) {
            res_handshake.body = "\"Sec-WebSocket-Key\" header not found";
            return false;
        }

        if (req_handshake.headers.find("sec-websocket-version") == req_handshake.headers.end()) {
            res_handshake.body = "\"Sec-WebSocket-Version\" header not found";
            return false;
        }
        if (req_handshake.headers.at("sec-websocket-version") != "13") {
            res_handshake.body = "Invalid \"Sec-WebSocket-Version\" header value";
            return false;
        }
        return true;
    }

    inline bool validate_handshake_response(const http_request_t &req_handshake, http_response_t &res_handshake) {
        if (res_handshake.headers.find("connection") == res_handshake.headers.end()) {
            res_handshake.body = "Connection header not found";
            return false;
        }
        if (res_handshake.headers.at("connection") != "Upgrade") {
            res_handshake.body = "\"Connection\" header value is not \"Upgrade\"";
            return false;
        }

        if (res_handshake.headers.find("upgrade") == res_handshake.headers.end()) {
            res_handshake.body = "Upgrade header not found";
            return false;
        }
        if (res_handshake.headers.at("upgrade") != "websocket") {
            res_handshake.body = "\"Upgrade\" header value is not \"websocket\"";
            return false;
        }

        if (res_handshake.headers.find("sec-websocket-accept") == res_handshake.headers.end()) {
            res_handshake.body = "Sec-WebSocket-Accept header not found";
            return false;
        }
        std::string req_key = req_handshake.headers.at("Sec-WebSocket-Key");
        std::string res_key = res_handshake.headers.at("sec-websocket-accept");
        std::string encoded_key = generate_accept_key(req_key);
        if (encoded_key != res_key) {
            res_handshake.body = "Invalid handshake: \"Sec-WebSocket-Accept\" is invalid.";
            return false;
        }

        return true;
    }
}
