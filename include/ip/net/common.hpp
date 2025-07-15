#pragma once

#define ASIO_NOEXCEPT

#include <asio.hpp>
#include <set>
#include <map>
#ifdef ENABLE_SSL
#include <asio/ssl.hpp>
#include <asio/ssl/stream.hpp>
#endif
#include "ip/tcp/tcpremote.hpp"

using namespace asio::ip;

namespace ip {
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




    // Client side

    struct udp_client_t {
        udp_client_t(): socket(context), resolver(context) {}

        asio::io_context context;
        udp::socket socket;
        udp::endpoint endpoint;
        udp::resolver resolver;
    };

    struct tcp_client_t {
        tcp_client_t(): socket(context), resolver(context) {}

        asio::io_context context;
        tcp::socket socket;
        tcp::endpoint endpoint;
        tcp::resolver resolver;
    };

#ifdef ENABLE_SSL
    struct tcp_client_ssl_t {
        tcp_client_ssl_t(): ssl_context(asio::ssl::context::tlsv13_client),
                            ssl_socket(context, ssl_context),
                            resolver(context) {}

        asio::io_context context;
        asio::ssl::context ssl_context;
        tcp::resolver resolver;
        tcp::endpoint endpoint;
        asio::ssl::stream<tcp::socket> ssl_socket;
    };
#endif

    // Server side

    struct udp_bind_options_t {
        std::string address;
        uint16_t port = 8080;
        protocol_type_e protocol = v4;
        bool reuse_address = true;
    };

    struct udp_server_t {
        udp_server_t(): socket(context) {}

        asio::io_context context;
        udp::socket socket;
        udp::endpoint remote_endpoint;
    };

    struct tcp_server_t {
        tcp_server_t(): acceptor(context) {}

        asio::io_context context;
        tcp::acceptor acceptor;
        std::set<std::shared_ptr<tcp_remote_c>> clients;
    };
}