#pragma once

#include "ip/net/common.hpp"
#include "ip/utils/package.hpp"

using namespace asio::ip;

namespace ip {
    class udp_server_c {
    public:
        udp_server_c() { recv_buffer.reserve(recv_buffer_size); }

        ~udp_server_c() {
            if (net.socket.is_open()) close();
        }

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * udp_server_c server;
         * server.local_endpoint();
         * @endcode
         */
        bool is_open() const { return net.socket.is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * udp_server_c server;
         * server.local_endpoint();
         * @endcode
         */
        udp::endpoint local_endpoint() const { return net.socket.local_endpoint(); }

        /**
         * Get the remote endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * udp_server_c server;
         * server.remote_endpoint();
         * @endcode
         */
        udp::endpoint remote_endpoint() const { return net.socket.remote_endpoint(); }

        /**
         * Return a const ref of the latest error code returned by asio.
         *
         * @par Example
         * @code
         * udp_server_c server;
         * asio::error_code ec = server.get_error_code();
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
         * udp_server_c server;
         * server.set_recv_buffer_size(16384);
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
         * udp_server_c server;
         * size_t size = server.get_recv_buffer_size();
         * @endcode
         */
        size_t get_recv_buffer_size() const { return recv_buffer_size; }

        /**
         * Broadcasts a buffer to an specific endpoint.
         * It returns false if socket is closed or if buffer is empty.
         *
         * @param message String to be send.
         *
         * @param endpoint Remote endpoint to send data.
         *
         * @par Example
         * @code
         * udp_server_c server;
         * server.open({ "", 8080, v4, true });
         *
         * udp::endpoint ep = ...; // You must store endpoint from 'on_message_event'
         * std::string message = "...";
         * server.send_to(message, ep);
         * @endcode
         */
        bool send_to(const std::string &message, const udp::endpoint &endpoint) {
            if (!net.socket.is_open() || message.empty())
                return false;

            net.socket.async_send_to(asio::buffer(message.c_str(),  message.size()),
                                        endpoint,
                                        [&](const asio::error_code &ec, size_t bytes_sent) {
                                            send_cb(ec, bytes_sent, endpoint);
                                        });
            return true;
        }

        /**
         * Broadcasts a buffer to an specific endpoint.
         * It returns false if socket is closed or if buffer is empty.
         *
         * @param buffer Data to be send.
         *
         * @param endpoint Remote endpoint to send data.
         *
         * @par Example
         * @code
         * udp_server_c server;
         * server.open({ "", 8080, v4, true });
         *
         * udp::endpoint ep = ...; // You must store endpoint from 'on_message_event'
         * std::vector<uint8_t> buffer = { 0, 1, 2... };
         * server.send_buffer_to(buffer, ep);
         * @endcode
         */
        bool send_buffer_to(const std::vector<uint8_t> &buffer, const udp::endpoint &endpoint) {
            if (!net.socket.is_open() || buffer.empty())
                return false;

            net.socket.async_send_to(asio::buffer(buffer.data(),  buffer.size()),
                                        endpoint,
                                        [&](const asio::error_code &ec, size_t bytes_sent) {
                                            send_cb(ec, bytes_sent, endpoint);
                                        });
            return true;
        }

        /**
         * Listen for datagram messages on a named port and optional address.
         * It returns false if socket is already open or if asio return any error code during the bind.
         *
         * @param bind_opts An struct specifying protocol parameters to be used.
         * If 'port' is not specified or is 0, the operating system will attempt to bind to a random port.
         * If 'address' is not specified, the operating system will attempt to listen on all addresses.
         *
         * @par Example
         * @code
         * udp_server_c server;
         * server.bind({ "", 8080, v4, true });
         * @endcode
         */
        bool bind(const server_bind_options_t &bind_opts = { "", 8080, v4, true }) {
            if (net.socket.is_open())
                return false;

            net.socket.open(bind_opts.protocol == v4 ? udp::v4() : udp::v6(),
                            error_code);
            if (error_code) {
                std::lock_guard guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }
            net.socket.set_option(asio::socket_base::reuse_address(bind_opts.reuse_address));
            const udp::endpoint endpoint = bind_opts.address.empty() ?
                                        udp::endpoint(bind_opts.protocol == v4
                                                        ? udp::v4()
                                                        : udp::v6(),
                                                        bind_opts.port) :
                                        udp::endpoint(
                                            make_address(bind_opts.address),
                                            bind_opts.port
                                        );
            net.socket.bind(endpoint, error_code);
            if (error_code) {
                std::lock_guard guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }

            asio::post(thread_pool, [&]{ run_context_thread(); });
            return true;
        }

        /**
         * Close the underlying socket and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @param force Cancel all asynchronous operations associated with the socket if true.
         *
         * @par Example
         * @code
         * udp_server_c server;
         * server.close();
         * @endcode
         */
        void close(const bool force = false) {
            is_closing.store(true);
            if (force) {
                if (net.socket.is_open()) {
                    net.socket.cancel();
                    {
                        std::lock_guard guard(mutex_error);
                        net.socket.close(error_code);
                        if (error_code && on_error)
                            on_error(error_code);
                    }
                }
            } else {
                if (net.socket.is_open()) {
                    std::lock_guard guard(mutex_error);
                    net.socket.shutdown(udp::socket::shutdown_both, error_code);
                    if (error_code && on_error)
                         on_error(error_code);
                    net.socket.close(error_code);
                    if (error_code && on_error)
                        on_error(error_code);
                }
            }
            net.context.stop();
            net.context.restart();
            if (on_close)
                on_close();
            is_closing.store(false);
        }

        /**
         * Adds the listener function to 'on_listening'.
         * This event will be triggered when socket start to listening.
         *
         * @par Example
         * @code
         * udp_server_c server;
         * server.on_listening = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_listening;

        /**
         * Adds the listener function to 'on_message_sent'.
         * This event will be triggered when a message has been sent.
         * 'error code' may be used to check if socket fail to send message.
         *
         * @par Example
         * @code
         * udp_server_c server;
         * server.on_message_sent = [&](const asio::error_code &ec, const size_t bytes_sent, const udp::endpoint &endpoint) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &, const size_t, const udp::endpoint &)> on_message_sent;

        /**
         * Adds the listener function to 'on_message_received'.
         * This event will be triggered when a message has been received.
         *
         * @par Example
         * @code
         * udp_server_c server;
         * server.on_message_received = [&](const std::vector<uint8_t> &buffer, const size_t bytes_recvd, const udp::endpoint &endpoint) {
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
         * udp_server_c server;
         * server.on_close = [&]() {
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
         * udp_server_c server;
         * server.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_error;
        std::atomic<bool> is_closing = false;
        udp_server_t net;
        asio::error_code error_code;
        size_t recv_buffer_size = 16384;
        std::vector<uint8_t> recv_buffer;

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            error_code.clear();
            consume_receive_buffer();
            net.socket.async_receive_from(asio::buffer(recv_buffer, recv_buffer.size()),
                                          net.remote_endpoint,
                                          [&](const asio::error_code &ec, const size_t bytes_recvd) { receive_from_cb(ec, bytes_recvd); });
            net.context.run();
            if (net.socket.is_open() && !is_closing.load())
                close();
        }

        void send_cb(const asio::error_code &error, const size_t bytes_sent, const udp::endpoint &endpoint) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_message_sent)
                    on_message_sent(error, bytes_sent, endpoint);
                return;
            }

            if (on_message_sent)
                on_message_sent(error, bytes_sent, endpoint);
        }

        void consume_receive_buffer() {
            if (!recv_buffer.empty()) 
                recv_buffer.clear();

            if (recv_buffer.capacity() != recv_buffer_size)
                recv_buffer.shrink_to_fit();

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
                on_message_received(recv_buffer, bytes_recvd, net.remote_endpoint);
                
            consume_receive_buffer();
            net.socket.async_receive_from(asio::buffer(recv_buffer, recv_buffer.size()),
                                            net.remote_endpoint,
                                            [&](const asio::error_code &ec, const size_t bytes_received) { receive_from_cb(ec, bytes_received); });
        }
    };
}