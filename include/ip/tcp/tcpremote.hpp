#pragma once

#include "ip/net/common.hpp"

using namespace asio::ip;

namespace ip {
    class tcp_remote_c {
    public:
        explicit tcp_remote_c(asio::io_context &io_context): socket(io_context) {
        }

        ~tcp_remote_c() {
            if (socket.is_open())
                close();
        }

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * tcp_remote_c client;
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return socket.is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * tcp_remote_c client;
         * asio::ip::tcp::endpoint ep = client.local_endpoint();
         * @endcode
         */
        tcp::endpoint local_endpoint() const { return socket.local_endpoint(); }

        /**
         * Get the remote endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * tcp_remote_c client;
         * asio::ip::tcp::endpoint ep = client.remote_endpoint();
         * @endcode
         */
        tcp::endpoint remote_endpoint() const { return socket.remote_endpoint(); }

        /// Just ignore this function
        tcp::socket &get_socket() { return socket; }

        /**
         * Return a const ref of the latest error code returned by asio.
         *
         * @par Example
         * @code
         * tcp_remote_c client;
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
         * @par Example
         * @code
         * tcp_remote_c client;
         * client.connect("localhost", 8080, v4);
         *
         * std::string message = "...";
         * client.write(message);
         * @endcode
         */
        bool write(const std::string &message) {
            if (!socket.is_open() || message.empty())
                return false;

            asio::async_write(socket,
                              asio::buffer(message.c_str(), message.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_cb(ec, bytes_sent);
                              });
            return true;
        }

        /**
         * Sends buffer data on the socket.
         * It returns false if socket is closed or if buffer is empty.
         *
         * @param buffer String to be send.
         *
         * @par Example
         * @code
         * tcp_remote_c client;
         * client.connect("localhost", 8080, v4);
         *
         * std::vector<uint8_t> buffer = { 0, 1, 2, ... };
         * client.write_buffer(buffer);
         * @endcode
         */
        bool write_buffer(const std::vector<uint8_t> &buffer) {
            if (!socket.is_open() || buffer.empty())
                return false;

            asio::async_write(socket,
                              asio::buffer(buffer.data(), buffer.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_cb(ec, bytes_sent);
                              });
            return true;
        }

        /// Ignore this function
        void connect() {
            asio::async_read(socket,
                             recv_buffer,
                             asio::transfer_at_least(1),
                             [&](const asio::error_code &ec, const size_t bytes_received) {
                                 std::cout << "read" << std::endl;
                                 read_cb(ec, bytes_received);
                             });
        }

        /**
         * Close the underlying socket and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @param force Cancel all asynchronous operations associated with the socket if true.
         *
         * @par Example
         * @code
         * tcp_remote_c client;
         * client.close();
         * @endcode
         */
        void close(const bool force = false) {
            if (force) {
                if (socket.is_open()) {
                    socket.cancel(); {
                        std::lock_guard guard(mutex_error);
                        socket.close(error_code);
                        if (error_code && on_error)
                            on_error(error_code);
                    }
                }
            } else {
                if (socket.is_open()) {
                    std::lock_guard guard(mutex_error);
                    socket.shutdown(tcp::socket::shutdown_both, error_code);
                    if (error_code && on_error)
                        on_error(error_code);
                    socket.close(error_code);
                    if (error_code && on_error)
                        on_error(error_code);
                }
            }
            if (on_close)
                on_close();
        }

        /**
         * Adds the listener function to 'on_message_sent'.
         * This event will be triggered when a message has been sent.
         * 'error_code' may be used to check if socket fail to send message.
         *
         * @par Example
         * @code
         * tcp_remote_c client;
         * client.on_message_sent = [&](const asio::error_code &ec, const size_t bytes_sent, const tcp::endpoint &endpoint) {
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
         * tcp_remote_c client;
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
         * tcp_remote_c client;
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
         * tcp_remote_c client;
         * client.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_error;
        tcp::socket socket;
        asio::error_code error_code;
        asio::streambuf recv_buffer;

        void write_cb(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                error_code = error;
                if (on_message_sent)
                    on_message_sent(error, bytes_sent);
                return;
            }

            if (on_message_sent)
                on_message_sent(error, bytes_sent);
        }

        void consume_recv_buffer() {
            const size_t size = recv_buffer.size();
            if (size > 0)
                recv_buffer.consume(size);
        }

        void read_cb(const asio::error_code &ec, std::size_t bytes_recvd) {
            if (ec) {
                if (ec == asio::error::eof) {
                    if (on_close)
                        on_close();
                    return;
                }
                if (on_error)
                    on_error(ec);
                if (on_close)
                    on_close();
                return;
            }

            if (on_message_received) {
                std::vector<uint8_t> buf;
                buf.resize(bytes_recvd);
                asio::buffer_copy(asio::buffer(buf, bytes_recvd),
                                  recv_buffer.data());
                on_message_received(buf, bytes_recvd);
            }

            consume_recv_buffer();
            asio::async_read(socket,
                             recv_buffer,
                             asio::transfer_at_least(1),
                             [&](const asio::error_code &ec, const size_t bytes_received) {
                                 read_cb(ec, bytes_received);
                             });
        }
    };
}
