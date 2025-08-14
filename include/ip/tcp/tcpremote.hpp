#pragma once

#include "ip/net/common.hpp"

using namespace asio::ip;

namespace internetprotocol {
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
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * tcp_remote_c client;
         *
         * std::string message = "...";
         * client.write(message);
         * @endcode
         */
        bool write(const std::string &message, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!socket.is_open() || message.empty())
                return false;

            asio::async_write(socket,
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
         * tcp_remote_c client;
         *
         * std::vector<uint8_t> buffer = { 0, 1, 2, ... };
         * client.write_buffer(buffer);
         * @endcode
         */
        bool write_buffer(const std::vector<uint8_t> &buffer, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!socket.is_open() || buffer.empty())
                return false;

            asio::async_write(socket,
                              asio::buffer(buffer.data(), buffer.size()),
                              [&, callback](const asio::error_code &ec, const size_t bytes_sent) {
                                  if (callback) callback(ec, bytes_sent);
                              });
            return true;
        }

        /// Ignore this function
        void connect() {
            asio::async_read(socket,
                             recv_buffer,
                             asio::transfer_at_least(1),
                             [&](const asio::error_code &ec, const size_t bytes_received) {
                                 read_cb(ec, bytes_received);
                             });
        }

        /**
         * Close the underlying socket and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @par Example
         * @code
         * tcp_remote_c client;
         * client.close();
         * @endcode
         */
        void close() {
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

        void consume_recv_buffer() {
            const size_t size = recv_buffer.size();
            if (size > 0)
                recv_buffer.consume(size);
        }

        void read_cb(const asio::error_code &error, std::size_t bytes_recvd) {
            if (error) {
                if (error == asio::error::eof) {
                    if (on_close)
                        on_close();
                    return;
                }
                if (on_error)
                    on_error(error);
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

#ifdef ENABLE_SSL
    class tcp_remote_ssl_c {
    public:
        tcp_remote_ssl_c(asio::io_context &io_context, asio::ssl::context &ssl_context)
            : ssl_socket(io_context, ssl_context) {
        }

        ~tcp_remote_ssl_c() {
            if (ssl_socket.next_layer().is_open())
                close();
        }

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * tcp_remote_ssl_c client;
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return ssl_socket.next_layer().is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * tcp_remote_ssl_c client;
         * asio::ip::tcp::endpoint ep = client.local_endpoint();
         * @endcode
         */
        tcp::endpoint local_endpoint() const { return ssl_socket.next_layer().local_endpoint(); }

        /**
         * Get the remote endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * tcp_remote_ssl_c client;
         * asio::ip::tcp::endpoint ep = client.remote_endpoint();
         * @endcode
         */
        tcp::endpoint remote_endpoint() const { return ssl_socket.next_layer().remote_endpoint(); }

        /// Just ignore this function
        asio::ssl::stream<tcp::socket> &get_socket() { return ssl_socket; }

        /**
         * Return a const ref of the latest error code returned by asio.
         *
         * @par Example
         * @code
         * tcp_remote_ssl_c client;
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
         * tcp_remote_ssl_c client;
         *
         * std::string message = "...";
         * client.write(message);
         * @endcode
         */
        bool write(const std::string &message, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!ssl_socket.next_layer().is_open() || message.empty())
                return false;

            asio::async_write(ssl_socket,
                              asio::buffer(message.c_str(), message.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
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
         * tcp_remote_ssl_c client;
         *
         * std::vector<uint8_t> buffer = { 0, 1, 2, ... };
         * client.write_buffer(buffer);
         * @endcode
         */
        bool write_buffer(const std::vector<uint8_t> &buffer, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!ssl_socket.next_layer().is_open() || buffer.empty())
                return false;

            asio::async_write(ssl_socket,
                              asio::buffer(buffer.data(), buffer.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  if (callback) callback(ec, bytes_sent);
                              });
            return true;
        }

        /// Ignore this function
        void connect() {
            ssl_socket.async_handshake(asio::ssl::stream_base::server,
                                       [&](const asio::error_code &ec) {
                                           ssl_handshake(ec);
                                       });
        }

        /**
         * Close the underlying socket and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @par Example
         * @code
         * tcp_remote_ssl_c client;
         * client.close();
         * @endcode
         */
        void close() {
            if (ssl_socket.next_layer().is_open()) {
                std::lock_guard guard(mutex_error);
                ssl_socket.lowest_layer().shutdown(asio::socket_base::shutdown_both, error_code);
                if (error_code && on_error)
                    on_error(error_code);
                ssl_socket.lowest_layer().close(error_code);
                if (error_code && on_error)
                    on_error(error_code);

                ssl_socket.shutdown(error_code);
                if (error_code && on_error)
                    on_error(error_code);
                ssl_socket.next_layer().close(error_code);
                if (error_code && on_error)
                    on_error(error_code);
            }
        }

        /**
         * Adds the listener function to 'on_message_received'.
         * This event will be triggered when a message has been received.
         *
         * @par Example
         * @code
         * tcp_remote_ssl_c client;
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
         * tcp_remote_ssl_c client;
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
         * tcp_remote_ssl_c client;
         * client.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_error;
        asio::ssl::stream<tcp::socket> ssl_socket;
        asio::error_code error_code;
        asio::streambuf recv_buffer;

        void ssl_handshake(const asio::error_code &error) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                if (on_close) on_close();
                return;
            }

            consume_recv_buffer();
            asio::async_read(ssl_socket,
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

        void read_cb(const asio::error_code &ec, std::size_t bytes_recvd) {
            if (ec) {
                if (ec == asio::error::eof) {
                    close();
                    if (on_close) on_close();
                    return;
                }
                if (on_error) on_error(ec);
                close();
                if (on_close) on_close();
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
            asio::async_read(ssl_socket,
                             recv_buffer,
                             asio::transfer_at_least(1),
                             [&](const asio::error_code &ec, const size_t bytes_received) {
                                 read_cb(ec, bytes_received);
                             });
        }
    };
#endif
}
