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

#include "Net/Type.hpp"

namespace InternetProtocol {
    static asio::thread_pool thread_pool(std::thread::hardware_concurrency());

	/*HTTP REQUEST*/
	enum class EMethod : uint8_t {
		DEL = 0,
		GET = 1,
		HEAD = 2,
		OPTIONS = 3,
		PATCH = 4,
		POST = 5,
		PUT = 6,
		TRACE = 7,
	};

    namespace Server {
        enum class EServerProtocol : uint8_t {
            V4 = 0,
            V6 = 1,
        };

        struct FAsioUdp {
            FAsioUdp() : socket(context) {
            }

            FAsioUdp(const FAsioUdp &asio) : socket(context) {
            }

            FAsioUdp &operator=(const FAsioUdp &asio) {
                if (this != &asio) {

                }
                return *this;
            }

            asio::io_context context;
            asio::ip::udp::socket socket;
            asio::ip::udp::endpoint remote_endpoint;
        };

        struct FAsioTcp {
            FAsioTcp() : acceptor(context) {
            }

            FAsioTcp(const FAsioTcp &asio) : acceptor(context) {
            }

            FAsioTcp &operator=(const FAsioTcp &asio) {
                if (this != &asio) {
                    acceptor = asio::ip::tcp::acceptor(context);
                    sockets = asio.sockets;
                }
                return *this;
            }

            asio::io_context context;
            asio::ip::tcp::acceptor acceptor;
            std::set<socket_ptr> sockets;
        };
#ifdef ASIO_USE_OPENSSL
		struct FAsioTcpSsl {
			FAsioTcpSsl() : acceptor(context), ssl_context(asio::ssl::context::tlsv13_server) {
			}

			FAsioTcpSsl(const FAsioTcpSsl& asio) : acceptor(context),
				ssl_context(asio::ssl::context::tlsv13_server) {
			}

			FAsioTcpSsl& operator=(const FAsioTcpSsl& asio) {
				if (this != &asio) {
					acceptor = asio::ip::tcp::acceptor(context);
					ssl_context = asio::ssl::context(asio::ssl::context::tlsv13_server);
				}
				return *this;
			}

		    asio::io_context context;
		    asio::ssl::context ssl_context;
		    asio::ip::tcp::acceptor acceptor;
		    std::set<ssl_socket_ptr> ssl_sockets;
		};
#endif
    	/*HTTP REQUEST*/
    	const std::map<std::string, EMethod> RequestMethod = {
    		{"DELETE", EMethod::DEL}, {"GET", EMethod::GET},
			{"HEAD", EMethod::HEAD}, {"OPTIONS", EMethod::OPTIONS},
			{"PATCH", EMethod::PATCH}, {"POST", EMethod::POST},
			{"PUT", EMethod::PUT}, {"TRACE", EMethod::TRACE},
		};

    	struct FRequest {
    		~FRequest() {

    		}
    		EMethod method = EMethod::GET;
    		std::string path = "/";
    		std::string version = "1.1";
    		std::map<std::string, std::vector<std::string>> headers;
    		std::string body;
    	};

    	struct FResponse {
    		~FResponse() {
    			headers.clear();
    		}
    		std::string version = "1.1";
    		std::map<std::string, std::string> headers;
    		std::string body;
    	};
    };

    namespace Client {
        /*UDP*/
        struct FAsioUdp {
            FAsioUdp() : socket(context),
                         resolver(context){
            }

            FAsioUdp(const FAsioUdp &asio) : resolver(context),
                                             socket(context),
                                             endpoints(asio.endpoints) {
            }

            FAsioUdp &operator=(const FAsioUdp &asio) {
                if (this != &asio) {
                    endpoints = asio.endpoints;
                }
                return *this;
            }

            asio::io_context context;
            asio::ip::udp::socket socket;
            asio::ip::udp::endpoint endpoints;
            asio::ip::udp::resolver resolver;
        };

        /*TCP*/
        struct FAsioTcp {
            FAsioTcp() : resolver(context),
                         socket(context) {
            }

            FAsioTcp(const FAsioTcp &asio) : resolver(context),
            socket(context) {
                endpoints = asio.endpoints;
            }

            FAsioTcp &operator=(const FAsioTcp &asio) {
                if (this != &asio) {
                    endpoints = asio.endpoints;
                }
                return *this;
            }

            asio::io_context context;
            asio::ip::tcp::resolver resolver;
            asio::ip::tcp::resolver::results_type endpoints;
            asio::ip::tcp::socket socket;
        };
#ifdef ASIO_USE_OPENSSL
		struct FAsioTcpSsl {
			FAsioTcpSsl()
				: context(),
				ssl_context(asio::ssl::context::tlsv13_client),
		        ssl_socket(context, ssl_context),
				resolver(context) {
				ssl_context.set_verify_mode(asio::ssl::verify_peer);
				ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(context, ssl_context);
			}

			FAsioTcpSsl(const FAsioTcpSsl& asio)
			: ssl_context(asio::ssl::context::tlsv13_client),
		    ssl_socket(context, ssl_context),
			resolver(context),
			endpoints(asio.endpoints) {
				ssl_context.set_verify_mode(asio::ssl::verify_peer);
				ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(context, ssl_context);
			}

			FAsioTcpSsl& operator=(const FAsioTcpSsl& asio) {
				if (this != &asio) {
					ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(context, ssl_context);
					endpoints = asio.endpoints;
				}
				return *this;
			}

		    asio::io_context context;
		    asio::ssl::context ssl_context;
		    asio::ip::tcp::resolver resolver;
		    asio::ip::tcp::resolver::results_type endpoints;
		    asio::ssl::stream<asio::ip::tcp::socket> ssl_socket;
		};
#endif
    	/*HTTP REQUEST*/
    	const std::map<EMethod, std::string> RequestMethod = {
    		{EMethod::DEL, "DELETE"}, {EMethod::GET, "GET"},
			{EMethod::HEAD, "HEAD"}, {EMethod::OPTIONS, "OPTIONS"},
			{EMethod::PATCH, "PATCH"}, {EMethod::POST, "POST"},
			{EMethod::PUT, "PUT"}, {EMethod::TRACE, "TRACE"},
		};

    	struct FRequest {
    		~FRequest() {
    			params.clear();
    			method = EMethod::GET;
    			path = "/";
    			version = "1.1";
    			headers.clear();
    			body.clear();
    		}
    		std::map<std::string, std::string> params;
    		EMethod method = EMethod::GET;
    		std::string path = "/";
    		std::string version = "1.1";
    		std::map<std::string, std::string> headers;
    		std::string body;
    	};

    	struct FResponse {
    		~FResponse() {
    			if (!headers.empty())
    				headers.clear();
    			content_lenght = 0;
    			body.clear();
    		}
    		std::map<std::string, std::vector<std::string> > headers;
    		int content_lenght = 0;
    		std::string body;
    	};
    }
	/*HTTP RESPONSE*/
	const std::map<int, std::string> ResponseStatusCode = {
        // 1xx Informational
        {100, "Continue"},
        {101, "Switching Protocols"},
        {102, "Processing"},
        {103, "Early Hints"},

        // 2xx Success
        {200, "OK"},
        {201, "Created"},
        {202, "Accepted"},
        {203, "Non-Authoritative Information"},
        {204, "No Content"},
        {205, "Reset Content"},
        {206, "Partial Content"},
        {207, "Multi-Status"},
        {208, "Already Reported"},
        {226, "IM Used"},

        // 3xx Redirection
        {300, "Multiple Choices"},
        {301, "Moved Permanently"},
        {302, "Found"},
        {303, "See Other"},
        {304, "Not Modified"},
        {305, "Use Proxy"},
        {306, "Switch Proxy"},
        {307, "Temporary Redirect"},
        {308, "Permanent Redirect"},

        // 4xx Client Error
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {402, "Payment Required"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {406, "Not Acceptable"},
        {407, "Proxy Authentication Required"},
        {408, "Request Timeout"},
        {409, "Conflict"},
        {410, "Gone"},
        {411, "Length Required"},
        {412, "Precondition Failed"},
        {413, "Payload Too Large"},
        {414, "URI Too Long"},
        {415, "Unsupported Media Type"},
        {416, "Range Not Satisfiable"},
        {417, "Expectation Failed"},
        {418, "I'm a teapot"},
        {421, "Misdirected Request"},
        {422, "Unprocessable Entity"},
        {423, "Locked"},
        {424, "Failed Dependency"},
        {425, "Too Early"},
        {426, "Upgrade Required"},
        {428, "Precondition Required"},
        {429, "Too Many Requests"},
        {431, "Request Header Fields Too Large"},
        {451, "Unavailable For Legal Reasons"},

        // 5xx Server Error
        {500, "Internal Server Error"},
        {501, "Not Implemented"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"},
        {504, "Gateway Timeout"},
        {505, "HTTP Version Not Supported"},
        {506, "Variant Also Negotiates"},
        {507, "Insufficient Storage"},
        {508, "Loop Detected"},
        {510, "Not Extended"},
        {511, "Network Authentication Required"}
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

