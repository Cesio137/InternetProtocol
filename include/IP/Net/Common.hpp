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

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <random>
#include <set>
#include <vector>

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE
#define ASIO_NO_DEPRECATED
#include <asio.hpp>
#ifdef ASIO_USE_OPENSSL
#include <asio/ssl.hpp>
#endif
namespace InternetProtocol {
    namespace Server {
        enum class EServerProtocol : uint8_t {
            V4 = 0,
            V6 = 1,
        };

        struct FAsioUdp {
            FAsioUdp() : socket(context) {
            }

            asio::error_code error_code;
            asio::io_context context;
            asio::ip::udp::socket socket;
            asio::ip::udp::endpoint remote_endpoint;

            FAsioUdp(const FAsioUdp &asio) : socket(
                context, asio::ip::udp::endpoint(asio.socket.local_endpoint().protocol(),
                                                 asio.socket.local_endpoint().port())) {
                error_code = asio.error_code;
            }

            FAsioUdp &operator=(const FAsioUdp &asio) {
                if (this != &asio) {
                    error_code = asio.error_code;
                }
                return *this;
            }
        };

        struct FAsioTcp {
            FAsioTcp() : acceptor(context) {
            }

            asio::error_code error_code;
            asio::io_context context;
            asio::ip::tcp::acceptor acceptor;
            std::set<std::shared_ptr<asio::ip::tcp::socket>> peers;

            FAsioTcp(const FAsioTcp &asio) : acceptor(context),
                                             error_code(asio.error_code) {
            }

            FAsioTcp &operator=(const FAsioTcp &asio) {
                if (this != &asio) {
                    error_code = asio.error_code;
                    acceptor = asio::ip::tcp::acceptor(context);
                }
                return *this;
            }
        };
#ifdef ASIO_USE_OPENSSL
        struct FAsioTcpSsl {
            FAsioTcpSsl() : acceptor(context), ssl_context(asio::ssl::context::sslv23) {
            }

            asio::error_code error_code;
            asio::io_context context;
            asio::ssl::context ssl_context;
            asio::ip::tcp::acceptor acceptor;
            std::set<std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>>> ssl_peers;

            FAsioTcpSsl(const FAsioTcp &asio) : acceptor(context),
                                             ssl_context(asio::ssl::context::sslv23),
                                             error_code(asio.error_code) {
            }

            FAsioTcpSsl &operator=(const FAsioTcpSsl &asio) {
                if (this != &asio) {
                    error_code = asio.error_code;
                    acceptor = asio::ip::tcp::acceptor(context);
                    ssl_context = asio::ssl::context(asio::ssl::context::sslv23);
                }
                return *this;
            }
        };
#endif
    };

    namespace Client {
        /*UDP*/
        struct FAsioUdp {
            FAsioUdp() : resolver(context), socket(context) {
            }

            asio::error_code error_code;
            asio::io_context context;
            asio::ip::udp::socket socket;
            asio::ip::udp::endpoint endpoints;
            asio::ip::udp::resolver resolver;
            uint8_t attemps_fail = 0;

            FAsioUdp(const FAsioUdp &asio) : resolver(context), socket(context) {
                error_code = asio.error_code;
                endpoints = asio.endpoints;
            }

            FAsioUdp &operator=(const FAsioUdp &asio) {
                if (this != &asio) {
                    error_code = asio.error_code;
                    endpoints = asio.endpoints;
                }
                return *this;
            }
        };

        /*TCP*/
        struct FAsioTcp {
            FAsioTcp() : resolver(context), socket(context) {
            }

            asio::error_code error_code;
            asio::io_context context;
            asio::ip::tcp::resolver resolver;
            asio::ip::tcp::resolver::results_type endpoints;
            asio::ip::tcp::socket socket;
            uint8_t attemps_fail = 0;

            FAsioTcp(const FAsioTcp &asio) : resolver(context), socket(context) {
                error_code = asio.error_code;
                endpoints = asio.endpoints;
            }

            FAsioTcp &operator=(const FAsioTcp &asio) {
                if (this != &asio) {
                    error_code = asio.error_code;
                    endpoints = asio.endpoints;
                }
                return *this;
            }
        };
#ifdef ASIO_USE_OPENSSL
        struct FAsioTcpSsl {
            FAsioTcpSsl()
                : context(),
                  ssl_context(asio::ssl::context::sslv23),
                  resolver(context),
                  ssl_socket(context, ssl_context) {
                ssl_context.set_verify_mode(asio::ssl::verify_peer);
            }

            asio::error_code error_code;
            asio::io_context context;
            asio::ssl::context ssl_context;
            asio::ip::tcp::resolver resolver;
            asio::ip::tcp::resolver::results_type endpoints;
            asio::ssl::stream<asio::ip::tcp::socket> ssl_socket;
            uint8_t attemps_fail = 0;

            FAsioTcpSsl(const FAsioTcpSsl &asio)
                : context(),
                  ssl_context(asio::ssl::context::sslv23),
                  resolver(context),
                  ssl_socket(context, ssl_context) {
                ssl_context.set_verify_mode(asio::ssl::verify_peer);
                error_code = asio.error_code;
                endpoints = asio.endpoints;
            }

            FAsioTcpSsl &operator=(const FAsioTcpSsl &asio) {
                if (this != &asio) {
                    ssl_context.set_verify_mode(asio::ssl::verify_peer);
                    error_code = asio.error_code;
                    endpoints = asio.endpoints;
                }
                return *this;
            }
        };
#endif
    }

    /*HTTP REQUEST*/
    enum class EVerb : uint8_t {
        GET = 0,
        POST = 1,
        PUT = 2,
        PATCH = 3,
        DEL = 4,
        COPY = 5,
        HEAD = 6,
        OPTIONS = 7,
        LOCK = 8,
        UNLOCK = 9,
        PROPFIND = 10
    };

    struct FRequest {
        std::map<std::string, std::string> params;
        EVerb verb = EVerb::GET;
        std::string path = "/";
        std::string version = "2.0";
        std::map<std::string, std::string> headers;
        std::string body;

        void clear() {
            params.clear();
            verb = EVerb::GET;
            path = "/";
            version = "2.0";
            headers.clear();
            body.clear();
        }
    };

    struct FResponse {
        std::map<std::string, std::vector<std::string> > headers;
        int contentLenght = 0;
        std::string content;

        void appendHeader(const std::string &headerline) {
            size_t pos = headerline.find(':');
            if (pos != std::string::npos) {
                std::string key = trimWhitespace(headerline.substr(0, pos));
                std::string value = trimWhitespace(headerline.substr(pos + 1));
                if (key == "Content-Length") {
                    contentLenght = std::stoi(value);
                    return;
                }
                std::vector<std::string> values = splitString(value, ';');
                std::transform(
                    values.begin(), values.end(), values.begin(),
                    [this](const std::string &str) { return trimWhitespace(str); });
                headers.insert_or_assign(key, values);
            }
        }

        void setContent(const std::string &value) {
            if (value.empty()) return;
            content = value;
        }

        void appendContent(const std::string &value) {
            if (value.empty()) return;
            content.append(value);
        }

        void clear() {
            headers.clear();
            contentLenght = 0;
            content.clear();
        }

    private:
        std::vector<std::string> splitString(const std::string &str, char delimiter) {
            std::vector<std::string> tokens;
            std::string token;
            std::istringstream tokenStream(str);
            while (std::getline(tokenStream, token, delimiter)) {
                tokens.push_back(token);
            }
            return tokens;
        }

        std::string trimWhitespace(const std::string &str) {
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
    };

    /*WEBSOCKET*/
    enum class EOpcode : uint8_t {
        FRAME_CON = 0x00,
        TEXT_FRAME = 0x01,
        BINARY_FRAME = 0x02,
        NON_CONTROL_FRAMES = 0x03,
        CONNECTION_CLOSE = 0x08,
        PING = 0x09,
        PONG = 0x0A,
        FURTHER_FRAMES = 0x0B,
    };

    enum class ERSV : uint8_t { RSV1 = 0x40, RSV2 = 0x20, RSV3 = 0x10 };

    struct FDataFrame {
        bool fin = true;
        bool rsv1 = false;
        bool rsv2 = false;
        bool rsv3 = false;
        bool mask = true;
        EOpcode opcode = EOpcode::TEXT_FRAME;
        size_t length = 0;
        std::array<std::byte, 4> masking_key;
    };

    struct FHandShake {
        std::string path = "chat";
        std::string version = "1.1";
        std::string Sec_WebSocket_Key = "dGhlIHNhbXBsZSBub25jZQ==";
        std::string origin = "client";
        std::string Sec_WebSocket_Protocol = "chat, superchat";
        std::string Sec_Websocket_Version = "13";
    };
} // namespace InternetProtocol
