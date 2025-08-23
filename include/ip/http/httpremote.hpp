/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
*/

#pragma once

#include "ip/net/common.hpp"
#include "ip/utils/net.hpp"

using namespace asio::ip;

namespace internetprotocol {
    class http_remote_c {
    public:
        http_remote_c(asio::io_context &io_context, const uint16_t timeout = 0): socket(io_context),
            idle_timer(io_context) { idle_timeout_seconds = timeout; }

        ~http_remote_c() {
            if (socket.is_open())
                close();
        }

        /**
         * Set/Get response headers.
         *
         * @par Example
         * @code
         * http_remote_c client;
         * http_response_t &headers = client.headers;
         * @endcode
         */
        http_response_t headers;

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * http_remote_c client;
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return socket.is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * http_remote_c client;
         * asio::ip::tcp::endpoint ep = server.local_endpoint();
         * @endcode
         */
        tcp::endpoint local_endpoint() const { return socket.local_endpoint(); }

        /**
         * Get the remote endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * http_remote_c client;
         * asio::ip::tcp::endpoint ep = server.remote_endpoint();
         * @endcode
         */
        tcp::endpoint remote_endpoint() const { return socket.remote_endpoint(); }

        /// Just ignore this function
        tcp::socket &get_socket() { return socket; }

        /**
         * Send response to client. Return false if socket is closed.
         *
         * @param callback This callback is triggered when a response has been received.
         *
         * @par Example
         * @code
         * http_remote_c client;
         * response.write([](const asio::error_code &ec, const size_t bytes_sent){
         *      // ...
         * });
         * @endcode
         */
        bool write(const std::function<void(const asio::error_code &, const size_t bytes_sent)> &callback = nullptr) {
            if (!socket.is_open())
                return false;

            reset_idle_timer();

            std::string payload = prepare_response(headers);
            asio::async_write(socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&, callback](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_cb(ec, bytes_sent, callback);
                              });
            return true;
        }

        /// Just ignore this function
        void connect() {
            start_idle_timer();
            asio::async_read_until(socket, recv_buffer, "\r\n",
                                   [&](const asio::error_code &ec, const size_t bytes_received) {
                                       read_cb(ec, bytes_received);
                                   });
        }

        /**
         * Close the underlying socket and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @param force Cancel all asynchronous operations associated with the client sockets if true.
         *
         * @par Example
         * @code
         * http_remote_c client;
         * client.close(false);
         * @endcode
         */
        void close() {
            is_closing.store(true);
            if (socket.is_open()) {
                std::lock_guard guard(mutex_error);
                socket.shutdown(tcp::socket::shutdown_both, error_code);
                if (error_code && on_error)
                    on_error(error_code);
                socket.close(error_code);
                if (error_code && on_error)
                    on_error(error_code);
            }
            is_closing.store(false);
        }

        /// Just ignore this event listener
        std::function<void(const http_request_t &)> on_request;

        /**
         * Adds the listener function to 'on_close'.
         * This event will be triggered after acceptor socket has been closed and all client has been disconnected.
         *
         * @par Example
         * @code
         * http_remote_c client;
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
         * http_remote_c client;
         * client.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_error;
        std::atomic<bool> is_closing = false;
        tcp::socket socket;
        asio::steady_timer idle_timer;
        uint16_t idle_timeout_seconds;
        asio::error_code error_code;
        bool will_close = false;
        asio::streambuf recv_buffer;

        void start_idle_timer() {
            if (idle_timeout_seconds == 0)
                return;

            idle_timer.expires_after(std::chrono::seconds(idle_timeout_seconds));
            idle_timer.async_wait([&](const asio::error_code &ec) {
                if (ec == asio::error::operation_aborted)
                    return;

                if (is_closing.load())
                    return;

                close();
                if (on_close) on_close();
            });
        }

        void reset_idle_timer() {
            if (is_closing.load() || idle_timeout_seconds == 0)
                return;

            idle_timer.cancel();
            start_idle_timer();
        }

        void consume_recv_buffer() {
            const size_t size = recv_buffer.size();
            if (size > 0)
                recv_buffer.consume(size);
        }

        void write_cb(const asio::error_code &error, const size_t bytes_sent,
                      const std::function<void(const asio::error_code &ec, const size_t bytes_sent)> &callback) {
            if (!will_close)
                reset_idle_timer();
            if (callback) callback(error, bytes_sent);
            if (will_close) {
                if (idle_timeout_seconds != 0)
                    idle_timer.cancel();
                close();
                if (on_close) on_close();
            }
        }

        void read_cb(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                consume_recv_buffer();
                if (on_error && error != asio::error::eof) on_error(error);
                if (!is_closing.load()) {
                    if (idle_timeout_seconds != 0)
                        idle_timer.cancel();
                    close();
                }
                if (on_close) on_close();
                return;
            }
            reset_idle_timer();

            std::istream request_stream(&recv_buffer);
            std::string method;
            std::string path;
            std::string version;

            request_stream >> method;
            request_stream >> path;
            request_stream >> version;

            if (version != "HTTP/1.0" && version != "HTTP/1.1" && version != "HTTP/2.0") {
                headers.status_code = 400;
                headers.status_message = "Bad Request";
                headers.body = "HTTP version not supported.";
                headers.headers.insert_or_assign("Content-Type", "text/plain");
                headers.headers.insert_or_assign("Content-Length", std::to_string(headers.body.size()));
                will_close = true;
                write();
                return;
            }

            if (string_to_request_method(method) == UNKNOWN) {
                headers.body = "Method not supported.";
                headers.headers.insert_or_assign("Allow", "DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT, TRACE");
                headers.headers.insert_or_assign("Content-Type", "text/plain");
                headers.headers.insert_or_assign("Content-Length", std::to_string(headers.body.size()));
                headers.status_code = 400;
                headers.status_message = "Bad Request";
                will_close = true;
                write();
                return;
            }

            version.erase(0, 5);
            recv_buffer.consume(2);
            asio::async_read_until(socket,
                                   recv_buffer, "\r\n\r\n",
                                   [&, path, version](const asio::error_code &ec, const size_t bytes_received) {
                                       read_headers(ec, path, version);
                                   });
        }

        void read_headers(const asio::error_code &error, const std::string &path, const std::string &version) {
            if (error) {
                consume_recv_buffer();
                if (on_error && error != asio::error::eof) on_error(error);
                if (!is_closing.load()) {
                    idle_timer.cancel();
                    close();
                }
                if (on_close) on_close();
                return;
            }
            http_request_t req;
            req.version = version;
            req.path = path;

            std::istream request_stream(&recv_buffer);
            std::string header;

            while (std::getline(request_stream, header) && header != "\r")
                req_append_header(req, header);

            if (recv_buffer.size() > 0) {
                std::ostringstream body_buffer;
                body_buffer << &recv_buffer;
                req.body = body_buffer.str();
            }

            headers.status_code = 200;
            headers.status_message = "OK";
            headers.headers.insert_or_assign("Content-Type", "text/plain");
            headers.headers.insert_or_assign("X-Powered-By", "ASIO");

            will_close = req.headers.find("Connection") != req.headers.end() ? req.headers.at("Connection") != "keep-alive" : true;
            if (!will_close)
                will_close = req.headers.find("connection") != req.headers.end() ? req.headers.at("connection") != "keep-alive" : true;

            consume_recv_buffer();
            if (on_request) on_request(req);
        }
    };

#ifdef ENABLE_SSL
    class http_remote_ssl_c {
    public:
        http_remote_ssl_c(asio::io_context &io_context, asio::ssl::context &ssl_context, const uint16_t timeout = 0)
        : ssl_socket(io_context, ssl_context), idle_timer(io_context) { idle_timeout_seconds = timeout; }

        ~http_remote_ssl_c() {
            if (ssl_socket.next_layer().is_open())
                close();
        }

        /**
         * Set/Get response headers.
         *
         * @par Example
         * @code
         * http_remote_ssl_c client;
         * http_response_t &headers = client.headers;
         * @endcode
         */
        http_response_t response;

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * http_remote_ssl_ssl_c client;
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return ssl_socket.next_layer().is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * http_remote_ssl_c client;
         * asio::ip::tcp::endpoint ep = server.local_endpoint();
         * @endcode
         */
        tcp::endpoint local_endpoint() const { return ssl_socket.next_layer().local_endpoint(); }

        /**
         * Get the remote endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * http_remote_ssl_c client;
         * asio::ip::tcp::endpoint ep = server.remote_endpoint();
         * @endcode
         */
        tcp::endpoint remote_endpoint() const { return ssl_socket.next_layer().remote_endpoint(); }

        /// Just ignore this function
        asio::ssl::stream<tcp::socket> &get_socket() { return ssl_socket; }

        /**
         * Send response to client. Return false if socket is closed.
         *
         * @param callback This callback is triggered when a response has been received.
         *
         * @par Example
         * @code
         * http_remote_ssl_c client;
         * response.write([](const asio::error_code &ec, const size_t bytes_sent){
         *      // ...
         * });
         * @endcode
         */
        bool write(const std::function<void(const asio::error_code &, const size_t bytes_sent)> &callback = nullptr) {
            if (!ssl_socket.next_layer().is_open())
                return false;

            reset_idle_timer();

            std::string payload = prepare_response(response);
            asio::async_write(ssl_socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&, callback](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_cb(ec, bytes_sent, callback);
                              });
            return true;
        }

        /// Just ignore this function
        void connect() {
            start_idle_timer();
            ssl_socket.async_handshake(asio::ssl::stream_base::server,
                                 [&](const asio::error_code &ec) {
                                     ssl_handshake(ec);
                                 });
        }

        /**
         * Close the underlying socket and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @param force Cancel all asynchronous operations associated with the client sockets if true.
         *
         * @par Example
         * @code
         * http_remote_ssl_c client;
         * client.close(false);
         * @endcode
         */
        void close() {
            is_closing.store(true);
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
            is_closing.store(false);
        }

        std::function<void(const http_request_t &)> on_request;

        /**
         * Adds the listener function to 'on_close'.
         * This event will be triggered after acceptor socket has been closed and all client has been disconnected.
         *
         * @par Example
         * @code
         * http_remote_ssl_c client;
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
         * http_remote_ssl_c client;
         * client.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_error;
        std::atomic<bool> is_closing = false;
        asio::ssl::stream<tcp::socket> ssl_socket;
        asio::steady_timer idle_timer;
        uint16_t idle_timeout_seconds = 0;
        asio::error_code error_code;
        bool will_close = false;
        asio::streambuf recv_buffer;

        void start_idle_timer() {
            if (idle_timeout_seconds == 0)
                return;

            idle_timer.expires_after(std::chrono::seconds(idle_timeout_seconds));
            idle_timer.async_wait([&](const asio::error_code &ec) {
                if (ec == asio::error::operation_aborted)
                    return;

                if (is_closing.load())
                    return;

                close();
                if (on_close) on_close();
            });
        }

        void reset_idle_timer() {
            if (is_closing.load() || idle_timeout_seconds == 0)
                return;

            idle_timer.cancel();
            start_idle_timer();
        }

        void consume_recv_buffer() {
            const size_t size = recv_buffer.size();
            if (size > 0)
                recv_buffer.consume(size);
        }

        void ssl_handshake(const asio::error_code &error) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                if (on_close) on_close();
                return;
            }

            asio::async_read_until(ssl_socket, recv_buffer, "\r\n",
                                   [&](const asio::error_code &ec, const size_t bytes_received) {
                                       read_cb(ec, bytes_received);
                                   });

        }

        void write_cb(const asio::error_code &error, const size_t bytes_sent,
                      const std::function<void(const asio::error_code &ec, const size_t bytes_sent)> &callback) {
            if (!will_close)
                reset_idle_timer();
            if (callback) callback(error, bytes_sent);
            if (will_close) {
                if (idle_timeout_seconds != 0)
                    idle_timer.cancel();
                close();
                if (on_close) on_close();
            }
        }

        void read_cb(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                consume_recv_buffer();
                if (on_error && error != asio::error::eof) on_error(error);
                if (!is_closing.load()) {
                    if (idle_timeout_seconds != 0)
                        idle_timer.cancel();
                    close();
                }
                if (on_close) on_close();
                return;
            }
            reset_idle_timer();

            std::istream request_stream(&recv_buffer);
            std::string method;
            std::string path;
            std::string version;

            request_stream >> method;
            request_stream >> path;
            request_stream >> version;

            if (version != "HTTP/1.0" && version != "HTTP/1.1" && version != "HTTP/2.0") {
                response.status_code = 400;
                response.status_message = "Bad Request";
                response.body = "HTTP version not supported.";
                response.headers.insert_or_assign("Content-Type", "text/plain");
                response.headers.insert_or_assign("Content-Length", std::to_string(response.body.size()));
                will_close = true;
                write();
                return;
            }

            if (string_to_request_method(method) == UNKNOWN) {
                response.body = "Method not supported.";
                response.headers.insert_or_assign("Allow", "DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT, TRACE");
                response.headers.insert_or_assign("Content-Type", "text/plain");
                response.headers.insert_or_assign("Content-Length", std::to_string(response.body.size()));
                response.status_code = 400;
                response.status_message = "Bad Request";
                will_close = true;
                write();
                return;
            }

            version.erase(0, 5);
            recv_buffer.consume(2);
            asio::async_read_until(ssl_socket,
                                   recv_buffer, "\r\n\r\n",
                                   [&, path, version](const asio::error_code &ec, const size_t bytes_received) {
                                       read_headers(ec, path, version);
                                   });
        }

        void read_headers(const asio::error_code &error, const std::string &path, const std::string &version) {
            if (error) {
                consume_recv_buffer();
                if (on_error && error != asio::error::eof) on_error(error);
                if (!is_closing.load()) {
                    idle_timer.cancel();
                    close();
                }
                if (on_close) on_close();
                return;
            }
            http_request_t req;
            req.version = version;
            req.path = path;

            std::istream request_stream(&recv_buffer);
            std::string header;

            while (std::getline(request_stream, header) && header != "\r")
                req_append_header(req, header);

            if (recv_buffer.size() > 0) {
                std::ostringstream body_buffer;
                body_buffer << &recv_buffer;
                req.body = body_buffer.str();
            }

            response.status_code = 200;
            response.status_message = "OK";
            response.headers.insert_or_assign("Content-Type", "text/plain");
            response.headers.insert_or_assign("X-Powered-By", "ASIO");

            will_close = req.headers.find("Connection") != req.headers.end() ? req.headers.at("Connection") != "keep-alive" : true;
            if (!will_close)
                will_close = req.headers.find("connection") != req.headers.end() ? req.headers.at("connection") != "keep-alive" : true;

            consume_recv_buffer();
            if (on_request) on_request(req);
        }
    };
#endif
}
