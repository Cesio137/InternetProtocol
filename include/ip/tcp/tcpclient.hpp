#pragma once

#include "ip/net/common.hpp"

namespace internetprotocol {
    struct tcp_client_t {
        tcp_client_t(): socket(context), resolver(context) {
        }

        asio::io_context context;
        tcp::socket socket;
        tcp::endpoint endpoint;
        tcp::resolver resolver;
    };

    class tcp_client_c {
    public:
        tcp_client_c() {}

        ~tcp_client_c() {
            if (net.socket.is_open())
                close();
            consume_recv_buffer();
        }

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * tcp_client_c client;
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return net.socket.is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * tcp_client_c client;
         * asio::ip::tcp::endpoint ep = client.local_endpoint();;
         * @endcode
         */
        tcp::endpoint local_endpoint() const { return net.socket.local_endpoint(); }

        /**
         * Get the remote endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * tcp_client_c client;
         * asio::ip::tcp::endpoint ep = client.remote_endpoint();
         * @endcode
         */
        tcp::endpoint remote_endpoint() const { return net.socket.remote_endpoint(); }

        /**
         * Return a const ref of the latest error code returned by asio.
         *
         * @par Example
         * @code
         * tcp_client_c client;
         * asio::error_code ec = client.get_error_code();
         * @endcode
         */
        const asio::error_code &get_error_code() const { return error_code; }


        /**
         * Sends string data on the socket.
         * It returns false if socket is closed or if buffer is empty.
         *
         * @param message String to be send.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * tcp_client_c client;
         * client.connect("localhost", 8080, v4);
         *
         * std::string message = "...";
         * client.write(message);
         * @endcode
         */
        bool write(const std::string &message, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!net.socket.is_open() || message.empty())
                return false;

            asio::async_write(net.socket,
                                asio::buffer(message.c_str(), message.size()),
                                [&, callback](const asio::error_code &ec, const size_t bytes_sent) {
                                    if (callback) callback(ec, bytes_sent);
                                });
            return true;
        }

        /**
         * Sends buffer data on the socket.
         * It returns false if socket is closed or if buffer is empty.
         *
         * @param buffer String to be send.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * tcp_client_c client;
         * client.connect("localhost", 8080, v4);
         *
         * std::vector<uint8_t> buffer = { 0, 1, 2, ... };
         * client.write_buffer(buffer);
         * @endcode
         */
        bool write_buffer(const std::vector<uint8_t> &buffer, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!net.socket.is_open() || buffer.empty())
                return false;

            asio::async_write(net.socket,
                                asio::buffer(buffer.data(), buffer.size()),
                                [&, callback](const asio::error_code &ec, const size_t bytes_sent) {
                                    if (callback) callback(ec, bytes_sent);
                                });
            return true;
        }

        /**
         * Initiate a connection on a given socket.
         * It returns false if socket is already open or if asio return any error code during the listening.
         *
         * @param address A string identifying a location. May be a descriptive name or a numeric address string. If an empty string and the passive flag has been specified, the resolved endpoints are suitable for local service binding.
         * @param port This may be a descriptive name or a numeric string corresponding to a port number. May be an empty string, in which case all resolved endpoints will have a port number of 0.
         * @param protocol A protocol object, normally representing either the IPv4 or IPv6 version of an internet protocol.
         *
         * @par Example
         * @code
         * tcp_client_c client;
         * client.connect("localhost", 8080, v4, true);
         * @endcode
         */
        bool connect(const std::string &address = "localhost", const std::string &port = "8080", const protocol_type_e protocol = v4) {
            if (net.socket.is_open())
                return false;

            net.resolver.async_resolve(protocol == v4 ? tcp::v4() : tcp::v6(),
                                        address, port,
                                        [&](const asio::error_code &ec, const tcp::resolver::results_type &results) {
                                            resolve(ec, results);
                                        });

            asio::post(thread_pool, [&]{ run_context_thread(); });
            return true;
        }

        /**
         * Close the underlying socket and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @par Example
         * @code
         * tcp_client_c client;
         * client.close();
         * @endcode
         */
        void close() {
            is_closing.store(true);
            if (net.socket.is_open()) {
                std::lock_guard guard(mutex_error);
                net.socket.shutdown(tcp::socket::shutdown_both, error_code);
                if (error_code && on_error)
                    on_error(error_code);
                net.socket.close(error_code);
                if (error_code && on_error)
                    on_error(error_code);
            }
            net.context.stop();
            net.context.restart();
            net.endpoint = tcp::endpoint();
            if (on_close)
                on_close();
            is_closing.store(false);
        }

        /**
         * Adds the listener function to 'on_connected'.
         * This event will be triggered when socket start to listening.
         *
         * @par Example
         * @code
         * tcp_client_c client;
         * client.on_connected = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_connected;

        /**
         * Adds the listener function to 'on_message_received'.
         * This event will be triggered when a message has been received.
         *
         * @par Example
         * @code
         * tcp_client_c client;
         * client.on_message_received = [&](const std::vector<uint8_t> &buffer, const size_t bytes_recvd) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const std::vector<uint8_t> &, const size_t)> on_message_received;

        /**
         * Adds the listener function to 'on_close'.
         * This event will be triggered when socket is closed.
         *
         * @par Example
         * @code
         * tcp_client_c client;
         * client.on_close = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_close;

        /**
         * Adds the listener function to 'on_error'.
         * This event will be triggered when any error has been returned by asio.
         *
         * @par Example
         * @code
         * tcp_client_c client;
         * client.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_error;
        std::atomic<bool> is_closing = false;
        tcp_client_t net;
        asio::error_code error_code;
        asio::streambuf recv_buffer;

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            error_code.clear();
            net.context.run();
            if (!is_closing.load())
                close();
        }

        void resolve(const asio::error_code &error, const tcp::resolver::results_type &results) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error)
                    on_error(error_code);
                return;
            }

            net.endpoint = results.begin()->endpoint();
            asio::async_connect(net.socket,
                                results,
                                [&](const asio::error_code &ec, const tcp::endpoint &ep) {
                                    conn(ec); 
                                });
        }

        void conn(const asio::error_code &error) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error)
                    on_error(error_code);
                return;
            }

            if (on_connected)
                on_connected();
            
            consume_recv_buffer();
            asio::async_read(net.socket,
                                recv_buffer, 
                                asio::transfer_at_least(1),
                                [&](const asio::error_code &ec, const size_t bytes_recvd) {
                                    read_cb(ec, bytes_recvd);
                                });
        }

        void consume_recv_buffer() {
            const size_t size = recv_buffer.size();
            if (size > 0)
                recv_buffer.consume(size);
        }

        void read_cb(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error)
                    on_error(error_code);
                return;
            }

            if (on_message_received) {
                std::vector<uint8_t> buffer;
                buffer.resize(bytes_recvd);
                asio::buffer_copy(asio::buffer(buffer, bytes_recvd),
                                    recv_buffer.data());
                on_message_received(buffer, bytes_recvd);
            }

            consume_recv_buffer();
            asio::async_read(net.socket,
                                recv_buffer, 
                                asio::transfer_at_least(1),
                                [&](const asio::error_code &ec, const size_t bytes_received) {
                                    read_cb(ec, bytes_received);
                                });
        }
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

    class tcp_client_ssl_c {
    public:
        tcp_client_ssl_c(const security_context_opts sec_opts = {}) {
            if (!sec_opts.private_key.empty()) {
                const asio::const_buffer buffer(sec_opts.private_key.data(), sec_opts.private_key.size());
                net.ssl_context.use_private_key(buffer, sec_opts.file_format);
            }

            if (!sec_opts.cert.empty()) {
                const asio::const_buffer buffer(sec_opts.cert.data(), sec_opts.cert.size());
                net.ssl_context.use_certificate(buffer, sec_opts.file_format);
            }

            if (!sec_opts.cert_chain.empty()) {
                const asio::const_buffer buffer(sec_opts.cert_chain.data(), sec_opts.cert_chain.size());
                net.ssl_context.use_certificate_chain(buffer);
            }

            if (!sec_opts.rsa_private_key.empty()) {
                const asio::const_buffer buffer(sec_opts.rsa_private_key.data(), sec_opts.rsa_private_key.size());
                net.ssl_context.use_rsa_private_key(buffer, sec_opts.file_format);
            }

            if (!sec_opts.host_name_verification.empty()) {
                net.ssl_context.set_verify_callback(asio::ssl::host_name_verification(sec_opts.host_name_verification));
            }

            switch (sec_opts.verify_mode) {
                case none:
                    net.ssl_context.set_verify_mode(asio::ssl::verify_none);
                    break;
                case verify_peer:
                    net.ssl_context.set_verify_mode(asio::ssl::verify_peer);
                    break;
                case verify_fail_if_no_peer_cert:
                    net.ssl_context.set_verify_mode(asio::ssl::verify_fail_if_no_peer_cert);
                    break;
                default:
                    break;
            }

            net.ssl_socket = asio::ssl::stream<tcp::socket>(net.context, net.ssl_context);
        }
        ~tcp_client_ssl_c() {
            if (net.ssl_socket.next_layer().is_open())
                close();
            consume_recv_buffer();
        }

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * tcp_client_ssl_c client({});
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return net.ssl_socket.next_layer().is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * tcp_client_ssl_c client({});
         * asio::ip::tcp::endpoint ep = client.local_endpoint();
         * @endcode
         */
        tcp::endpoint local_endpoint() const { return net.ssl_socket.next_layer().local_endpoint(); }

        /**
         * Get the remote endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * tcp_client_ssl_c client({});
         * asio::ip::tcp::endpoint ep = client.remote_endpoint();
         * @endcode
         */
        tcp::endpoint remote_endpoint() const { return net.ssl_socket.next_layer().remote_endpoint(); }

        /**
         * Return a const ref of the latest error code returned by asio.
         *
         * @par Example
         * @code
         * tcp_client_ssl_c client({});
         * asio::error_code ec = client.get_error_code();
         * @endcode
         */
        asio::error_code get_error_code() const { return error_code; }

        /**
         * Sends string data on the socket.
         * It returns false if socket is closed or if buffer is empty.
         *
         * @param message String to be send.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * tcp_client_ssl_c client({});
         * client.connect("localhost", 8080, v4);
         *
         * std::string message = "...";
         * client.write(message);
         * @endcode
         */
        bool write(const std::string &message, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!net.ssl_socket.next_layer().is_open() || message.empty())
                return false;

            asio::async_write(net.ssl_socket,
                                asio::buffer(message.c_str(), message.size()),
                                [&, callback](const asio::error_code &ec, const size_t bytes_sent) {
                                    if (callback) callback(ec, bytes_sent);
                                });
            return true;
        }

        /**
         * Sends string data on the socket.
         * It returns false if socket is closed or if buffer is empty.
         *
         * @param buffer String to be send.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * tcp_client_ssl_c client({});
         * client.connect("localhost", 8080, v4);
         *
         * std::vector<uint8_t> buffer = { 0, 1, 2, ... };
         * client.write_buffer(buffer);
         * @endcode
         */
        bool write_buffer(const std::vector<uint8_t> &buffer, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!net.ssl_socket.next_layer().is_open() || buffer.empty())
                return false;

            asio::async_write(net.ssl_socket,
                                asio::buffer(buffer.data(), buffer.size()),
                                [&, callback](const asio::error_code &ec, const size_t bytes_sent) {
                                    if (callback) callback(ec, bytes_sent);
                                });
            return true;
        }

        /**
         * Initiate a connection on a given socket.
         * It returns false if socket is already open or if asio return any error code during the listening.
         *
         * @param address A string identifying a location. May be a descriptive name or a numeric address string. If an empty string and the passive flag has been specified, the resolved endpoints are suitable for local service binding.
         * @param port This may be a descriptive name or a numeric string corresponding to a port number. May be an empty string, in which case all resolved endpoints will have a port number of 0.
         * @param protocol A protocol object, normally representing either the IPv4 or IPv6 version of an internet protocol.
         *
         * @par Example
         * @code
         * tcp_client_ssl_c client({});
         * client.connect("localhost", 8080, v4, true);
         * @endcode
         */
        bool connect(const std::string &address = "localhost", const std::string &port = "8080", const protocol_type_e protocol = v4) {
            if (net.ssl_socket.next_layer().is_open())
                return false;

            net.resolver.async_resolve(address,
                                        port,
                                        [&](const asio::error_code &ec, const tcp::resolver::results_type &results) {
                                            resolve(ec, results);
                                        });

            asio::post(thread_pool, [&]{ run_context_thread(); });
            return true;
        }

        /**
         * Close the underlying socket and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @par Example
         * @code
         * tcp_client_ssl_c client({});
         * client.close(false);
         * @endcode
         */
        void close() {
            is_closing.store(true);
            if (net.ssl_socket.next_layer().is_open()) {
                std::lock_guard guard(mutex_error);
                net.ssl_socket.lowest_layer().shutdown(asio::socket_base::shutdown_both, error_code);
                if (error_code && on_error)
                    on_error(error_code);
                net.ssl_socket.lowest_layer().close(error_code);
                if (error_code && on_error)
                    on_error(error_code);

                net.ssl_socket.shutdown(error_code);
                if (error_code && on_error)
                    on_error(error_code);
                net.ssl_socket.next_layer().close(error_code);
                if (error_code && on_error)
                    on_error(error_code);
            }
            net.context.stop();
            net.context.restart();
            net.endpoint = tcp::endpoint();
            net.ssl_socket = asio::ssl::stream<tcp::socket>(net.context, net.ssl_context);
            is_closing.store(true);
        }

        /**
         * Adds the listener function to 'on_connected'.
         * This event will be triggered when socket start to listening.
         *
         * @par Example
         * @code
         * tcp_client_ssl_c client({});
         * client.on_connected = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_connected;

        /**
         * Adds the listener function to 'on_message_received'.
         * This event will be triggered when a message has been received.
         *
         * @par Example
         * @code
         * tcp_client_ssl_c client({});
         * client.on_message_received = [&](const std::vector<uint8_t> &buffer, const size_t bytes_recvd) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const std::vector<uint8_t> &, const size_t)> on_message_received;

        /**
         * Adds the listener function to 'on_close'.
         * This event will be triggered when socket is closed.
         *
         * @par Example
         * @code
         * tcp_client_ssl_c client({});
         * client.on_close = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_close;

        /**
         * Adds the listener function to 'on_error'.
         * This event will be triggered when any error has been returned by asio.
         *
         * @par Example
         * @code
         * tcp_client_ssl_c client({});
         * client.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_error;
        std::atomic<bool> is_closing = false;
        tcp_client_ssl_t net;
        asio::error_code error_code;
        asio::streambuf recv_buffer;

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            error_code.clear();
            net.context.run();
            if (!is_closing.load())
                close();
        }

        void resolve(const asio::error_code &error, const tcp::resolver::results_type &results) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error)
                    on_error(error);
                return;
            }
            net.endpoint = results.begin()->endpoint();
            asio::async_connect(net.ssl_socket.lowest_layer(),
                                results,
                                [&](const asio::error_code &ec, const tcp::endpoint &ep) {
                                    conn(ec);
                                });
        }

        void conn(const asio::error_code &error) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error)
                    on_error(error);
                return;
            }

            net.ssl_socket.async_handshake(asio::ssl::stream_base::client,
                                            [&](const asio::error_code &ec) {
                                                ssl_handshake(ec);
                                            });
        }

        void ssl_handshake(const asio::error_code &error) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error)
                    on_error(error);
                return;
            }

            if (on_connected)
                on_connected();

            consume_recv_buffer();
            asio::async_read(net.ssl_socket,
                            recv_buffer,
                            asio::transfer_at_least(1),
                            [&](const asio::error_code &ec, const size_t bytes_received) {
                                read_cb(ec, bytes_received);
                            });

        }

        void consume_recv_buffer() {
            const size_t size = recv_buffer.size();
            if (size > 0)
                recv_buffer.consume(size);
        }

        void read_cb(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error)
                    on_error(error);
                return;
            }

            if (on_message_received) {
                std::vector<uint8_t> buffer;
                buffer.resize(bytes_recvd);
                asio::buffer_copy(asio::buffer(buffer, bytes_recvd),
                                  recv_buffer.data());
                on_message_received(buffer, bytes_recvd);
            }

            consume_recv_buffer();
            asio::async_read(net.ssl_socket,
                            recv_buffer,
                            asio::transfer_at_least(1),
                            [&](const asio::error_code &ec, const size_t bytes_received) {
                                read_cb(ec, bytes_received);
                            });
        }
    };
#endif
}