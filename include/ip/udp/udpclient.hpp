/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
*/

#pragma once

#include "ip/net/common.hpp"
#include "ip/utils/package.hpp"

using namespace asio::ip;

namespace internetprotocol {
    struct udp_client_t {
        udp_client_t(): socket(context), resolver(context) {
        }

        asio::io_context context;
        udp::socket socket;
        udp::endpoint endpoint;
        udp::resolver resolver;
    };

    class udp_client_c {
    public:
        udp_client_c() { recv_buffer.reserve(recv_buffer_size); }

        ~udp_client_c() {
            net.resolver.cancel();
            if (net.socket.is_open()) close();
        }

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * udp_client_c client;
         * client.local_endpoint();
         * @endcode
         */
        bool is_open() const { return net.socket.is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * udp_client_c client;
         * client.local_endpoint();
         * @endcode
         */
        udp::endpoint local_endpoint() const { return net.socket.local_endpoint(); }

        /**
         * Get the remote endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * udp_client_c client;
         * client.remote_endpoint();
         * @endcode
         */
        udp::endpoint remote_endpoint() const { return net.socket.remote_endpoint(); }

        /**
         * Return a const ref of the latest error code returned by asio.
         *
         * @par Example
         * @code
         * udp_client_c client;
         * asio::error_code ec = client.get_error_code();
         * @endcode
         */
        const asio::error_code &get_error_code() const { return error_code; }

        /**
         * Set receive buffer size in bytes
         *
         * @param size Size of buffer
         *
         * @par Example
         * @code
         * udp_client_c client;
         * client.set_recv_buffer_size(16384);
         * @endcode
         */
        void set_recv_buffer_size(const size_t size = 16384) {
            recv_buffer_size = size;
        }

        /**
         * Get receive buffer size in bytes
         *
         * @par Example
         * @code
         * udp_client_c client;
         * size_t size = client.get_recv_buffer_size();
         * @endcode
         */
        size_t get_recv_buffer_size() const { return recv_buffer_size; }

        /**
         * Broadcasts a buffer to an specific endpoint.
         * It returns false if socket is closed or if buffer is empty.
         *
         * @param message String to be send.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * udp_client_c client;
         * client.connect("localhost", 8080, v4);
         *
         * std::string message = "...";
         * client.send(message);
         * @endcode
         */
        bool send(const std::string &message, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!net.socket.is_open() || message.empty())
                return false;

            net.socket.async_send_to(asio::buffer(message.c_str(), message.size()),
                                        net.endpoint,
                                        [&, callback](const asio::error_code &ec, size_t bytes_sent) {
                                            if (callback) callback(ec, bytes_sent);
                                        });
            return true;
        }

        /**
         * Broadcasts a buffer to an specific endpoint.
         * It returns false if socket is closed or if buffer is empty.
         *
         * @param buffer Data to be send.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * udp_client_c client;
         * client.connect("localhost", 8080, v4);
         *
         * udp::endpoint ep = ...; // You must store endpoint from 'on_message_event'
         * std::vector<uint8_t> buffer = { 0, 1, 2... };
         * client.send_buffer(buffer);
         * @endcode
         */
        bool send_buffer(const std::vector<uint8_t> &buffer, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!net.socket.is_open() || buffer.empty())
                return false;

            net.socket.async_send_to(asio::buffer(buffer.data(), buffer.size()),
                                        net.endpoint,
                                        [&, callback](const asio::error_code &ec, size_t bytes_sent) {
                                            if (callback) callback(ec, bytes_sent);
                                        });
            return true;
        }

        /**
         * Listen for datagram messages on a named port and address.
         * It returns false if socket is already open or if asio return any error code during the bind.
         *
         * @param address A string identifying a location. May be a descriptive name or a numeric address string. If an empty string and the passive flag has been specified, the resolved endpoints are suitable for local service binding.
         * @param port This may be a descriptive name or a numeric string corresponding to a port number. May be an empty string, in which case all resolved endpoints will have a port number of 0.
         * @param protocol A protocol object, normally representing either the IPv4 or IPv6 version of an internet protocol.
         *
         * @par Example
         * @code
         * udp_client_c client;
         * client.connect("", 8080, v4, true);
         * @endcode
         */
        bool connect(const std::string &address = "localhost", const std::string &port = "8080", const protocol_type_e protocol = v4) {
            if (net.socket.is_open())
                return false;

            net.resolver.async_resolve(protocol == v4 ? udp::v4() : udp::v6(),
                                        address, port,
                                        [&](const asio::error_code &ec, const udp::resolver::results_type &results) {
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
         * udp_client_c client;
         * client.close();
         * @endcode
         */
        void close() {
            is_closing.store(true);
            if (net.socket.is_open()) {
                std::lock_guard guard(mutex_error);
                net.socket.shutdown(udp::socket::shutdown_both, error_code);
                if (error_code && on_error)
                    on_error(error_code);
                net.socket.close(error_code);
                if (error_code && on_error)
                    on_error(error_code);
            }
            net.context.stop();
            net.context.restart();
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
         * udp_client_c client;
         * client.on_connected = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_connected;

        /**
         * Adds the listener function to 'on_message_sent'.
         * This event will be triggered when a message has been sent.
         * 'error_code' may be used to check if socket fail to send message.
         *
         * @par Example
         * @code
         * udp_client_c client;
         * client.on_message_sent = [&](const asio::error_code &ec, const size_t bytes_sent, const udp::endpoint &endpoint) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &, const size_t)> on_message_sent;

        /**
         * Adds the listener function to 'on_message_received'.
         * This event will be triggered when a message has been received.
         *
         * @par Example
         * @code
         * udp_client_c client;
         * client.on_message_received = [&](const std::vector<uint8_t> &buffer, const size_t bytes_recvd, const udp::endpoint &endpoint) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const std::vector<uint8_t> &, const size_t, const udp::endpoint &)> on_message_received;

        /**
         * Adds the listener function to 'on_close'.
         * This event will be triggered when socket is closed.
         *
         * @par Example
         * @code
         * udp_client_c client;
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
         * udp_client_c client;
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
        udp_client_t net;
        asio::error_code error_code;
        size_t recv_buffer_size = 16384;
        std::vector<uint8_t> recv_buffer;

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            error_code.clear();
            net.context.run();
            if (net.socket.is_open() && !is_closing.load())
                close();
        }

        void resolve(const asio::error_code &error, const udp::resolver::results_type &results) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            net.endpoint = results.begin()->endpoint();
            net.socket.async_connect(net.endpoint,
                                        [&](const asio::error_code &ec) {
                                            conn(ec);
                                        });
        }

        void conn(const asio::error_code &error) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }

            if (on_connected)
                on_connected();

            consume_receive_buffer();
            net.socket.async_receive_from(asio::buffer(recv_buffer, recv_buffer.size()),
                                            net.endpoint,
                                            [&](const asio::error_code &ec, const size_t bytes_recvd) {
                                                receive_from_cb(ec, bytes_recvd);
                                            });
        }

        void consume_receive_buffer() {
            if (!recv_buffer.empty()) 
                recv_buffer.clear();

            if (recv_buffer.capacity() != recv_buffer_size)
                recv_buffer.shrink_to_fit();

            if (recv_buffer.size() != recv_buffer_size)
                recv_buffer.resize(recv_buffer_size);
        }

        void receive_from_cb(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }

            if (on_message_received)
                on_message_received(recv_buffer, bytes_recvd, net.endpoint);

            consume_receive_buffer();
            net.socket.async_receive_from(asio::buffer(recv_buffer, recv_buffer.size()),
                                            net.endpoint,
                                            [&](const asio::error_code &ec, const size_t bytes_received) {
                                                receive_from_cb(ec, bytes_received);
                                            });
        }
    };
}