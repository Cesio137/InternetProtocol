/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
*/

#pragma once

#define ASIO_NOEXCEPT

#include <asio.hpp>
#include <set>
#include <map>
#ifdef ENABLE_SSL
#include <asio/ssl.hpp>
#include <asio/ssl/stream.hpp>
#endif

using namespace asio::ip;

namespace internetprotocol {
    inline extern asio::thread_pool thread_pool(std::thread::hardware_concurrency());

    /**
     * This function stops the threads as soon as possible. As a result of calling stop_threads(), pending function objects may be never be invoked.
     */
    inline void stop_threads() {
        thread_pool.stop();
    }

    /**
     * This function blocks until the threads in the pool have completed. If join_threads() is not called prior to join_threads(), the join_threads() call will wait until the pool has no more outstanding work.
     */
    inline void join_threads() {
        thread_pool.join();
    }

    typedef enum : uint8_t {
        v4 = 0,
        v6 = 1,
    } protocol_type_e;

    typedef enum : uint8_t {
        none = 0x00,
        verify_peer = 0x01,
        verify_fail_if_no_peer_cert = 0x02,
        verify_client_once = 0x04,
    } verify_mode_e;

    typedef asio::ssl::context_base::file_format file_format_e;

    /**
     * @brief Security context configuration options
     *
     * This structure stores the necessary settings to establish a security context
     * for secure communications, including certificates and private keys.
     * All certificate and key values are loaded directly from memory rather than files.
     */
    struct security_context_opts {
        /// Private key content in memory. Can be left empty
        std::string private_key;

        /// Certificate content in memory. Can be left empty
        std::string cert;

        /// Certificate chain content in memory. Can be left empty
        std::string cert_chain;

        /// RSA private key content in memory. Can be left empty
        std::string rsa_private_key;

        /// Format of the certificate and key data (default: PEM)
        file_format_e file_format = file_format_e::pem;

        /// Verification mode (default: verify_peer)
        verify_mode_e verify_mode = verify_peer;

        /// Hostname for verification. Can be left empty
        std::string host_name_verification;
    };

    // HTTP
    typedef enum : uint8_t {
        UNKNOWN = 0,
        DEL = 1,
        GET = 2,
        HEAD = 3,
        OPTIONS = 4,
        PATCH = 5,
        POST = 6,
        PUT = 7,
    } request_method_e;

    struct http_request_t {
        request_method_e method = GET;
        std::string path = "/";
        std::string version = "1.1";
        std::map<std::string, std::string> params;
        std::map<std::string, std::string> headers;
        std::string body;
    };

    struct http_response_t {
        int status_code = 200;
        std::string status_message;
        std::string version = "1.1";
        std::map<std::string, std::string> headers;
        std::string body;
    };

    static std::map<int, std::string> response_status_t = {
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

    typedef enum  : uint8_t {
        TEXT_FRAME = 0x01,
        BINARY_FRAME = 0x02,
        CLOSE_FRAME = 0x08,
        PING = 0x09,
        PONG = 0x0A
    } opcode_e;

    typedef enum : uint8_t {
        RSV1 = 0x40,
        RSV2 = 0x20,
        RSV3 = 0x10
    } RSV_e;

    struct dataframe_t {
        bool fin = true;
        bool rsv1 = false;
        bool rsv2 = false;
        bool rsv3 = false;
        bool mask = true;
        opcode_e opcode = TEXT_FRAME;
        size_t length = 0;
        std::array<uint8_t, 4> masking_key{};
    };

    typedef enum : uint8_t  {
        CLOSED = 0,
        CLOSING = 1,
        OPEN = 2
    } close_state_e;

    // Client side
    struct client_bind_options_t {
        std::string address;
        std::string port = "8080";
        protocol_type_e protocol = v4;
    };

    struct udp_client_t {
        udp_client_t(): socket(context), resolver(context) {
        }

        asio::io_context context;
        udp::socket socket;
        udp::endpoint endpoint;
        udp::resolver resolver;
    };

    struct tcp_client_t {
        tcp_client_t(): socket(context), resolver(context) {
        }

        asio::io_context context;
        tcp::socket socket;
        tcp::endpoint endpoint;
        tcp::resolver resolver;
    };
#ifdef ENABLE_SSL
    struct tcp_client_ssl_t {
        tcp_client_ssl_t(): ssl_context(asio::ssl::context::tlsv13_client),
                            ssl_socket(context, ssl_context),
                            resolver(context) {
        }

        asio::io_context context;
        asio::ssl::context ssl_context;
        tcp::resolver resolver;
        tcp::endpoint endpoint;
        asio::ssl::stream<tcp::socket> ssl_socket;
    };
#endif

    // Server side
    struct server_bind_options_t {
        std::string address;
        uint16_t port = 8080;
        protocol_type_e protocol = v4;
        bool reuse_address = true;
    };

    struct udp_server_t {
        udp_server_t(): socket(context) {
        }

        asio::io_context context;
        udp::socket socket;
        udp::endpoint remote_endpoint;
    };

    template<typename T>
    struct tcp_server_t {
        tcp_server_t(): acceptor(context) {
        }

        asio::io_context context;
        tcp::acceptor acceptor;
        std::set<std::shared_ptr<T> > clients;
    };

#ifdef ENABLE_SSL
    template<typename T>
    struct tcp_server_ssl_t {
        tcp_server_ssl_t(): acceptor(context), ssl_context(asio::ssl::context::tlsv13) {
        }

        asio::io_context context;
        asio::ssl::context ssl_context;
        tcp::acceptor acceptor;
        std::set<std::shared_ptr<T>> ssl_clients;
    };
#endif
}