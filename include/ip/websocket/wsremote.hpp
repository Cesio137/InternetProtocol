/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
*/

#pragma once

#include "ip/net/common.hpp"
#include "ip/utils/dataframe.hpp"
#include "ip/utils/handshake.hpp"

using namespace asio::ip;

namespace internetprotocol {
    class ws_remote_c {
    public:
        explicit ws_remote_c(asio::io_context &io_context) : idle_timer(io_context), socket(io_context) {
            handshake.status_code = 101;
            handshake.status_message = "Switching Protocols";
            handshake.headers.insert_or_assign("Upgrade", "websocket");
            handshake.headers.insert_or_assign("Connection", "Upgrade");
            handshake.headers.insert_or_assign("Sec-WebSocket-Accept", "");
        }

        ~ws_remote_c() {
            if (socket.is_open())
                close();
        }

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * ws_remote_c client;
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return socket.is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * ws_remote_c client;
         * asio::ip::tcp::endpoint ep = client.local_endpoint();
         * @endcode
         */
        tcp::endpoint local_endpoint() const { return socket.local_endpoint(); }

        /**
         * Get the remote endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * ws_remote_c client;
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
         * ws_remote_c client;
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
         * @param dataframe Custom dataframe if needed.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * ws_remote_c client;
         *
         * std::string message = "...";
         * client.write(message);
         * @endcode
         */
        bool write(const std::string &message, const dataframe_t &dataframe = {}, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!is_open() || message.empty() || close_state.load() != OPEN) return false;

            dataframe_t frame = dataframe;
            frame.opcode = TEXT_FRAME;
            frame.mask = false;
            std::string payload = encode_string_payload(message, frame);
            asio::async_write(socket,
                              asio::buffer(payload.data(), payload.size()),
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
         * @param dataframe Custom dataframe if needed.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * ws_remote_c client;
         *
         * std::vector<uint8_t> buffer = { 0, 1, 2, ... };
         * client.write_buffer(buffer);
         * @endcode
         */
        bool write_buffer(const std::vector<uint8_t> &buffer, const dataframe_t &dataframe = {}, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!is_open() || buffer.empty() || close_state.load() != OPEN) return false;

            dataframe_t frame = dataframe;
            frame.opcode = BINARY_FRAME;
            frame.mask = false;
            std::vector<uint8_t> payload = encode_buffer_payload(buffer, frame);
            asio::async_write(socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  if (callback) callback(ec, bytes_sent);
                              });
            return true;
        }

        /**
         * Send a ping message.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * ws_remote_c client;
         * client.ping();
         * @endcode
         */
        bool ping(const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!is_open() || close_state.load() != OPEN) return false;

            dataframe_t dataframe;
            dataframe.opcode = PING;
            dataframe.mask = false;
            std::vector<uint8_t> payload = encode_buffer_payload({}, dataframe);
            asio::async_write(socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  if (callback) callback(ec, bytes_sent);
                              });
            return true;
        }

        /**
         * Send a pong message. This is already performed automatically.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * ws_remote_c client;
         * client.pong();
         * @endcode
         */
        bool pong(const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!is_open()) return false;

            dataframe_t dataframe;
            dataframe.opcode = PONG;
            dataframe.mask = false;
            std::vector<uint8_t> payload = encode_buffer_payload({}, dataframe);
            asio::async_write(socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  if (callback) callback(ec, bytes_sent);
                              });
            return true;
        }

        /// Ignore this function
        void connect() {
            close_state.store(OPEN);
            asio::async_read_until(socket,
                                   recv_buffer, "\r\n",
                                   [&](const asio::error_code &ec, const size_t bytes_received) {
                                       read_handshake_cb(ec, bytes_received);
                                   });
        }

        /**
         * Perform a smooth close of socket following ws protocol and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @param code Closing code.
         *
         * @param reason Reason.
         *
         * @par Example
         * @code
         * ws_remote_c client;
         * client.end();
         * @endcode
         */
        void end(const uint16_t code = 1000, const std::string &reason = "") {
            if (close_state.load() == CLOSED) return;

            if (close_state.load() == OPEN) {
                // Server iniciating close
                close_state.store(CLOSING);
                send_close_frame(code, reason);
            } else if (close_state.load() == CLOSING) {
                // Already in closing state, just perform final close
                close(code, reason);
            }
        }

        /**
         * Close the underlying socket and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @param code Closing code.
         *
         * @param reason Reason.
         *
         * @par Example
         * @code
         * ws_remote_c client;
         * client.close();
         * @endcode
         */
        void close(const uint16_t code = 1000, const std::string &reason = "") {
            if (close_state.load() == CLOSED) return;

            close_state.store(CLOSED);
            wait_close_frame_response.store(true);

            if (socket.is_open()) {
                bool is_locked = mutex_error.try_lock();

                socket.shutdown(tcp::socket::shutdown_both, error_code);
                if (error_code && error_code != asio::error::not_connected && on_error)
                    on_error(error_code);

                socket.close(error_code);
                if (error_code && on_error)
                    on_error(error_code);

                if (on_close) {
                    on_close(code, reason);
                }

                if (is_locked)
                    mutex_error.unlock();
            }
        }

        /**
         * Adds the listener function to 'on_connected'.
         * This event will be triggered when socket start to listening.
         *
         * @par Example
         * @code
         * ws_remote_c client();
         * client.on_connected = [&](const http_response_t &server_handshake) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const http_request_t &)> on_connected;

        /**
         * Adds the listener function to 'on_unexpected_handshake'.
         * This event will be triggered when a bad handshake has been received from server.
         *
         * @par Example
         * @code
         * ws_remote_c client();
         * client.on_unexpected_handshake = [&](const http_response_t &server_handshake) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const http_request_t &)> on_unexpected_handshake;

        /**
         * Adds the listener function to 'on_message_received'.
         * This event will be triggered when a message has been received.
         *
         * @par Example
         * @code
         * ws_remote_c client();
         * client.on_message_received = [&](const std::vector<uint8_t> &buffer, bool is_binary) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const std::vector<uint8_t> &, bool)> on_message_received;

        /**
         * Adds the listener function to 'on_ping'.
         * This event will be triggered when a ping has been received.
         *
         * @par Example
         * @code
         * ws_remote_c client();
         * client.on_ping = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_ping;

        /**
         * Adds the listener function to 'on_pong'.
         * This event will be triggered when a pong has been received.
         *
         * @par Example
         * @code
         * ws_remote_c client();
         * client.on_pong = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_pong;

        /**
         * Adds the listener function to 'on_close'.
         * This event will be triggered when socket is closed.
         *
         * @par Example
         * @code
         * tcp_client_c client();
         * client.on_close = [&](const uint16_t code, const std::string &reason) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const uint16_t, const std::string &)> on_close;

        /**
         * Adds the listener function to 'on_error'.
         * This event will be triggered when any error has been returned by asio.
         *
         * @par Example
         * @code
         * ws_remote_c client();
         * client.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_error;
        std::atomic<close_state_e> close_state = CLOSED;
        std::atomic<bool> wait_close_frame_response = true;
        asio::steady_timer idle_timer;
        tcp::socket socket;
        asio::error_code error_code;
        asio::streambuf recv_buffer;
        http_response_t handshake;
        bool close_frame_sent = false;

        void start_idle_timer() {
            idle_timer.expires_after(std::chrono::seconds(5));
            idle_timer.async_wait([&](const asio::error_code &ec) {
                if (ec == asio::error::operation_aborted)
                    return;

                if (close_state.load() == CLOSED)
                    return;

                close(1000, "");
            });
        }

        // Send close frame
        void send_close_frame(const uint16_t code, const std::string &reason) {
            if (!socket.is_open() || close_frame_sent) {
                close(code, reason);
                return;
            }

            close_frame_sent = true;
            dataframe_t dataframe;
            dataframe.opcode = CLOSE_FRAME;
            dataframe.mask = false;

            std::vector<uint8_t> close_payload;
            // Status code (big-endian)
            close_payload.push_back(static_cast<uint8_t>(code >> 8));
            close_payload.push_back(static_cast<uint8_t>(code & 0xFF));

            // Reason
            if (!reason.empty()) {
                close_payload.insert(close_payload.end(), reason.begin(), reason.end());
            }

            std::vector<uint8_t> encoded_payload = encode_buffer_payload(close_payload, dataframe);
            asio::async_write(socket,
                              asio::buffer(encoded_payload.data(), encoded_payload.size()),
                              [this, code, reason](const asio::error_code &ec, const size_t bytes_sent) {
                                  close_frame_sent_cb(ec, bytes_sent, code, reason);
                              });
        }

        // Callback for close frame sent
        void close_frame_sent_cb(const asio::error_code &error, const size_t bytes_sent,
                                 const uint16_t code, const std::string &reason) {
            if (error) {
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                close();
            }

            if (!wait_close_frame_response.load()) {
                end(code, reason);
                return;
            }
            start_idle_timer();
            asio::async_read(socket,
                                recv_buffer,
                                asio::transfer_at_least(1),
                                [&](const asio::error_code &ec, const size_t bytes_recvd) {
                                    read_cb(ec, bytes_recvd);
                                });
        }

        void read_handshake_cb(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                std::lock_guard lock(mutex_error);
                consume_recv_buffer();
                error_code = error;
                if (on_error) on_error(error);
                close(1002, "Error trying to read handshake");
                return;
            }

            http_request_t request;
            http_response_t response;
            std::istream response_stream(&recv_buffer);
            std::string method;
            std::string path;
            std::string version;
            response_stream >> method;
            response_stream >> path;
            response_stream >> version;
            version.erase(0, 5);

            request.method = string_to_request_method(method);
            request.path = path;

            if (method != "GET") {
                consume_recv_buffer();
                response.status_code = 405;
                response.status_message = "Method Not Allowed";
                std::string payload = prepare_response(response);
                asio::async_write(socket,
                                  asio::buffer(payload.data(), payload.size()),
                                  [&](const asio::error_code &ec, const size_t bytes_sent) {
                                      close(1002, "Protocol error");
                                  });
                if (on_unexpected_handshake) on_unexpected_handshake(request);
                return;
            }

            if (version != "1.1") {
                consume_recv_buffer();
                response.status_code = 505;
                response.status_message = "HTTP Version Not Supported";
                std::string payload = prepare_response(response);
                asio::async_write(socket,
                                  asio::buffer(payload.data(), payload.size()),
                                  [&](const asio::error_code &ec, const size_t bytes_sent) {
                                      close(1002, "Protocol error");
                                  });
                if (on_unexpected_handshake) on_unexpected_handshake(request);
                return;
            }

            recv_buffer.consume(2);
            asio::async_read_until(socket,
                                   recv_buffer, "\r\n\r\n",
                                   std::bind(&ws_remote_c::read_headers, this, asio::placeholders::error, request));
        }

        void read_headers(const asio::error_code &error, http_request_t &request) {
            if (error) {
                std::lock_guard lock(mutex_error);
                consume_recv_buffer();
                error_code = error;
                if (on_error) on_error(error);
                close(1002, "Error trying to read handshake header");
                return;
            }
            std::istream response_stream(&recv_buffer);
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                req_append_header(request, header);

            consume_recv_buffer();

            if (!validate_handshake_request(request, handshake)) {
                handshake.headers.erase("Upgrade");
                handshake.headers.erase("Connection");
                handshake.headers.erase("Sec-WebSocket-Accept");
                std::string payload = prepare_response(handshake);
                asio::async_write(socket,
                                  asio::buffer(payload.data(), payload.size()),
                                  [&](const asio::error_code &ec, const size_t bytes_sent) {
                                      close(1002, "Protocol error");
                                  });
                if (on_unexpected_handshake) on_unexpected_handshake(request);
                return;
            }

            std::string key = request.headers.at("sec-websocket-key");
            std::string accept = generate_accept_key(key);
            handshake.headers.insert_or_assign("Sec-WebSocket-Accept", accept);
            std::string payload = prepare_response(handshake);
            asio::async_write(socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_handshake_cb(ec, bytes_sent, request);
                              });
        }

        void write_handshake_cb(const asio::error_code &error, const size_t bytes_sent, const http_request_t &request) {
            if (error) {
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error)
                    on_error(error);
                close(1006, "Abnormal closure");
                return;
            }

            if (on_connected) on_connected(request);

            asio::async_read(socket,
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

        void read_cb(const asio::error_code &error, std::size_t bytes_recvd) {
            if (error) {
                std::lock_guard lock(mutex_error);
                consume_recv_buffer();
                error_code = error;
                if (on_error) on_error(error);
                uint16_t close_code = 1006;
                if (error == asio::error::eof) {
                    close_code = 1000; // Normal closure
                } else if (error == asio::error::connection_reset) {
                    close_code = 1006; // Abnormal closure
                }

                close(close_code, "Connection error");
                return;
            }

            std::vector<uint8_t> buffer;
            buffer.resize(bytes_recvd);
            asio::buffer_copy(asio::buffer(buffer, bytes_recvd),
                              recv_buffer.data());

            dataframe_t dataframe;
            std::vector<uint8_t> payload{};
            if (decode_payload(buffer, payload, dataframe)) {
                if (!dataframe.mask) {
                    consume_recv_buffer();
                    end(1002, "Protocol error - payload mask not found");
                    return;
                }
                switch (dataframe.opcode) {
                    case TEXT_FRAME:
                        if (on_message_received) on_message_received(payload, false);
                        break;
                    case BINARY_FRAME:
                        if (on_message_received) on_message_received(payload, true);
                        break;
                    case PING:
                        if (on_ping) on_ping();
                        pong();
                        break;
                    case PONG:
                        if (on_pong) on_pong();
                        break;
                    case CLOSE_FRAME:
                        uint16_t close_code = 1000;
                        std::string close_reason = "Shutdown connection";
                        if (payload.size() >= 2) {
                            close_code = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                            if (payload.size() > 2) {
                                close_reason = std::string(payload.begin() + 2, payload.end());
                            }
                        }
                        wait_close_frame_response.store(close_state.load() == CLOSING);
                        end(close_code, close_reason);
                        return;
                }
            } else {
                consume_recv_buffer();
                end(1002, "Protocol error - failed to decode payload");
                return;
            }

            consume_recv_buffer();

            if (close_state == OPEN) {
                asio::async_read(socket,
                                 recv_buffer,
                                 asio::transfer_at_least(1),
                                 [&](const asio::error_code &ec, const size_t bytes_received) {
                                     read_cb(ec, bytes_received);
                                 });
            }
        }
    };

#ifdef ENABLE_SSL
    class ws_remote_ssl_c {
    public:
        explicit ws_remote_ssl_c(asio::io_context &io_context, asio::ssl::context &ssl_context) : ssl_socket(io_context, ssl_context), idle_timer(io_context) {
            handshake.status_code = 101;
            handshake.status_message = "Switching Protocols";
            handshake.headers.insert_or_assign("Upgrade", "websocket");
            handshake.headers.insert_or_assign("Connection", "Upgrade");
            handshake.headers.insert_or_assign("Sec-WebSocket-Accept", "");
        }

        ~ws_remote_ssl_c() {
            if (ssl_socket.next_layer().is_open())
                close();
        }

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return ssl_socket.next_layer().is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         * asio::ip::tcp::endpoint ep = client.local_endpoint();
         * @endcode
         */
        tcp::endpoint local_endpoint() const { return ssl_socket.next_layer().local_endpoint(); }

        /**
         * Get the remote endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
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
         * ws_remote_ssl_c client();
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
         * @param dataframe Custom dataframe if needed.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         *
         * std::string message = "...";
         * client.write(message);
         * @endcode
         */
        bool write(const std::string &message, const dataframe_t &dataframe = {}, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!is_open() || message.empty() || close_state.load() != OPEN) return false;

            dataframe_t frame = dataframe;
            frame.opcode = TEXT_FRAME;
            frame.mask = false;
            std::string payload = encode_string_payload(message, frame);
            asio::async_write(ssl_socket,
                              asio::buffer(payload.data(), payload.size()),
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
         * @param dataframe Custom dataframe if needed.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         *
         * std::vector<uint8_t> buffer = { 0, 1, 2, ... };
         * client.write_buffer(buffer);
         * @endcode
         */
        bool write_buffer(const std::vector<uint8_t> &buffer, const dataframe_t &dataframe = {}, const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!is_open() || buffer.empty() || close_state.load() != OPEN) return false;

            dataframe_t frame = dataframe;
            frame.opcode = BINARY_FRAME;
            frame.mask = false;
            std::vector<uint8_t> payload = encode_buffer_payload(buffer, frame);
            asio::async_write(ssl_socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  if (callback) callback(ec, bytes_sent);
                              });
            return true;
        }

        /**
         * Send a ping message.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         * client.ping();
         * @endcode
         */
        bool ping(const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!is_open() || close_state.load() != OPEN) return false;

            dataframe_t dataframe;
            dataframe.opcode = PING;
            dataframe.mask = false;
            std::vector<uint8_t> payload = encode_buffer_payload({}, dataframe);
            asio::async_write(ssl_socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  if (callback) callback(ec, bytes_sent);
                              });
            return true;
        }

        /**
         * Send a pong message. This is already performed automatically.
         *
         * @param callback An optional callback function may be specified to as a way of reporting DNS errors or for determining when it is safe to reuse the buf object.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         * client.pong();
         * @endcode
         */
        bool pong(const std::function<void(const asio::error_code &, const size_t)> &callback = nullptr) {
            if (!is_open()) return false;

            dataframe_t dataframe;
            dataframe.opcode = PONG;
            dataframe.mask = false;
            std::vector<uint8_t> payload = encode_buffer_payload({}, dataframe);
            asio::async_write(ssl_socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  if (callback) callback(ec, bytes_sent);
                              });
            return true;
        }

        /// Ignore this function
        void connect() {
            close_state.store(OPEN);
            ssl_socket.async_handshake(asio::ssl::stream_base::server,
                                       [&](const asio::error_code &ec) {
                                           ssl_handshake(ec);
                                       });
        }

        /**
         * Perform a smooth close of socket following ws protocol and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @param code Closing code.
         *
         * @param reason Reason.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         * client.end();
         * @endcode
         */
        void end(const uint16_t code = 1000, const std::string &reason = "") {
            if (close_state.load() == CLOSED) return;

            if (close_state.load() == OPEN) {
                // Server iniciating close
                close_state.store(CLOSING);
                send_close_frame(code, reason);
            } else if (close_state.load() == CLOSING) {
                // Already in closing state, just perform final close
                close(code, reason);
            }
        }

        /**
         * Close the underlying socket and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @param code Closing code.
         *
         * @param reason Reason.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         * client.close();
         * @endcode
         */
        void close(const uint16_t code = 1000, const std::string &reason = "") {
            if (close_state.load() == CLOSED) return;

            close_state.store(CLOSED);
            wait_close_frame_response.store(true);

            if (ssl_socket.next_layer().is_open()) {
                bool is_locked = mutex_error.try_lock();

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

                if (is_locked)
                    mutex_error.unlock();
            }

            if (on_close) {
                on_close(code, reason);
            }
        }

        /**
         * Adds the listener function to 'on_connected'.
         * This event will be triggered when socket start to listening.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         * client.on_connected = [&](const http_response_t &server_handshake) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const http_request_t &)> on_connected;

        /**
         * Adds the listener function to 'on_unexpected_handshake'.
         * This event will be triggered when a bad handshake has been received from server.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         * client.on_unexpected_handshake = [&](const http_response_t &server_handshake) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const http_request_t &)> on_unexpected_handshake;

        /**
         * Adds the listener function to 'on_message_received'.
         * This event will be triggered when a message has been received.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         * client.on_message_received = [&](const std::vector<uint8_t> &buffer, bool is_binary) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const std::vector<uint8_t> &, bool)> on_message_received;

        /**
         * Adds the listener function to 'on_ping'.
         * This event will be triggered when a ping has been received.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         * client.on_ping = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_ping;

        /**
         * Adds the listener function to 'on_pong'.
         * This event will be triggered when a pong has been received.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         * client.on_pong = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_pong;

        /**
         * Adds the listener function to 'on_close'.
         * This event will be triggered when socket is closed.
         *
         * @par Example
         * @code
         * tcp_client_c client();
         * client.on_close = [&](const uint16_t code, const std::string &reason) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const uint16_t, const std::string &)> on_close;

        /**
         * Adds the listener function to 'on_error'.
         * This event will be triggered when any error has been returned by asio.
         *
         * @par Example
         * @code
         * ws_remote_ssl_c client();
         * client.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_error;
        std::atomic<close_state_e> close_state = CLOSED;
        std::atomic<bool> wait_close_frame_response = true;
        asio::steady_timer idle_timer;
        asio::ssl::stream<tcp::socket> ssl_socket;
        asio::error_code error_code;
        asio::streambuf recv_buffer;
        http_response_t handshake;
        bool close_frame_sent = false;

        void start_idle_timer() {
            idle_timer.expires_after(std::chrono::seconds(5));
            idle_timer.async_wait([&](const asio::error_code &ec) {
                if (ec == asio::error::operation_aborted)
                    return;

                if (close_state.load() == CLOSED)
                    return;

                close(1000, "");
            });
        }

        // Send close frame
        void send_close_frame(const uint16_t code, const std::string &reason) {
            if (!ssl_socket.next_layer().is_open() || close_frame_sent) {
                close(code, reason);
                return;
            }

            close_frame_sent = true;
            dataframe_t dataframe;
            dataframe.opcode = CLOSE_FRAME;
            dataframe.mask = false;

            std::vector<uint8_t> close_payload;
            // Status code (big-endian)
            close_payload.push_back(static_cast<uint8_t>(code >> 8));
            close_payload.push_back(static_cast<uint8_t>(code & 0xFF));

            // Reason
            if (!reason.empty()) {
                close_payload.insert(close_payload.end(), reason.begin(), reason.end());
            }

            std::vector<uint8_t> encoded_payload = encode_buffer_payload(close_payload, dataframe);
            asio::async_write(ssl_socket,
                              asio::buffer(encoded_payload.data(), encoded_payload.size()),
                              [this, code, reason](const asio::error_code &ec, const size_t bytes_sent) {
                                  close_frame_sent_cb(ec, bytes_sent, code, reason);
                              });
        }

        // Callback for close frame sent
        void close_frame_sent_cb(const asio::error_code &error, const size_t bytes_sent,
                                 const uint16_t code, const std::string &reason) {
            if (error) {
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
            }

            if (!wait_close_frame_response.load()) {
                end(code, reason);
                return;
            }
            start_idle_timer();
            asio::async_read(ssl_socket,
                                recv_buffer,
                                asio::transfer_at_least(1),
                                [&](const asio::error_code &ec, const size_t bytes_recvd) {
                                    read_cb(ec, bytes_recvd);
                                });
        }

        void ssl_handshake(const asio::error_code &error) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                if (on_close) on_close(1002, "SSL/TLS handshake failed");
                return;
            }

            asio::async_read_until(ssl_socket,
                                   recv_buffer, "\r\n",
                                   [&](const asio::error_code &ec, const size_t bytes_received) {
                                       read_handshake_cb(ec, bytes_received);
                                   });
        }

        void read_handshake_cb(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                std::lock_guard lock(mutex_error);
                consume_recv_buffer();
                error_code = error;
                if (on_error) on_error(error);
                close(1002, "Error trying to read handshake");
                return;
            }

            http_request_t request;
            http_response_t response;
            std::istream response_stream(&recv_buffer);
            std::string method;
            std::string path;
            std::string version;
            response_stream >> method;
            response_stream >> path;
            response_stream >> version;
            version.erase(0, 5);

            request.method = string_to_request_method(method);
            request.path = path;

            if (method != "GET") {
                consume_recv_buffer();
                response.status_code = 405;
                response.status_message = "Method Not Allowed";
                std::string payload = prepare_response(response);
                asio::async_write(ssl_socket,
                                  asio::buffer(payload.data(), payload.size()),
                                  [&](const asio::error_code &ec, const size_t bytes_sent) {
                                      close(1002, "Protocol error");
                                  });
                if (on_unexpected_handshake) on_unexpected_handshake(request);
                return;
            }

            if (version != "1.1") {
                consume_recv_buffer();
                response.status_code = 505;
                response.status_message = "HTTP Version Not Supported";
                std::string payload = prepare_response(response);
                asio::async_write(ssl_socket,
                                  asio::buffer(payload.data(), payload.size()),
                                  [&](const asio::error_code &ec, const size_t bytes_sent) {
                                      close(1002, "Protocol error");
                                  });
                if (on_unexpected_handshake) on_unexpected_handshake(request);
                return;
            }

            recv_buffer.consume(2);
            asio::async_read_until(ssl_socket,
                                   recv_buffer, "\r\n\r\n",
                                   std::bind(&ws_remote_ssl_c::read_headers, this, asio::placeholders::error, request));
        }

        void read_headers(const asio::error_code &error, http_request_t &request) {
            if (error) {
                std::lock_guard lock(mutex_error);
                consume_recv_buffer();
                error_code = error;
                if (on_error) on_error(error);
                close(1002, "Error trying to read handshake header");
                return;
            }
            std::istream response_stream(&recv_buffer);
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                req_append_header(request, header);

            consume_recv_buffer();

            if (!validate_handshake_request(request, handshake)) {
                handshake.headers.erase("Upgrade");
                handshake.headers.erase("Connection");
                handshake.headers.erase("Sec-WebSocket-Accept");
                std::string payload = prepare_response(handshake);
                asio::async_write(ssl_socket,
                                  asio::buffer(payload.data(), payload.size()),
                                  [&](const asio::error_code &ec, const size_t bytes_sent) {
                                      close(1002, "Protocol error");
                                  });
                if (on_unexpected_handshake) on_unexpected_handshake(request);
                return;
            }

            std::string key = request.headers.at("sec-websocket-key");
            std::string accept = generate_accept_key(key);
            handshake.headers.insert_or_assign("Sec-WebSocket-Accept", accept);
            std::string payload = prepare_response(handshake);
            asio::async_write(ssl_socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_handshake_cb(ec, bytes_sent, request);
                              });
        }

        void write_handshake_cb(const asio::error_code &error, const size_t bytes_sent, const http_request_t &request) {
            if (error) {
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error)
                    on_error(error);
                close(1006, "Abnormal closure");
                return;
            }

            if (on_connected) on_connected(request);

            asio::async_read(ssl_socket,
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

        void read_cb(const asio::error_code &error, std::size_t bytes_recvd) {
            if (error) {
                std::lock_guard lock(mutex_error);
                consume_recv_buffer();
                error_code = error;
                if (on_error) on_error(error);
                uint16_t close_code = 1006;
                if (error == asio::error::eof) {
                    close_code = 1000; // Normal closure
                } else if (error == asio::error::connection_reset) {
                    close_code = 1006; // Abnormal closure
                }

                close(close_code, "Connection error");
                return;
            }

            std::vector<uint8_t> buffer;
            buffer.resize(bytes_recvd);
            asio::buffer_copy(asio::buffer(buffer, bytes_recvd),
                              recv_buffer.data());

            dataframe_t dataframe;
            std::vector<uint8_t> payload{};
            if (decode_payload(buffer, payload, dataframe)) {
                if (!dataframe.mask) {
                    consume_recv_buffer();
                    end(1002, "Protocol error - payload mask not found");
                    return;
                }
                switch (dataframe.opcode) {
                    case TEXT_FRAME:
                        if (on_message_received) on_message_received(payload, false);
                        break;
                    case BINARY_FRAME:
                        if (on_message_received) on_message_received(payload, true);
                        break;
                    case PING:
                        if (on_ping) on_ping();
                        pong();
                        break;
                    case PONG:
                        if (on_pong) on_pong();
                        break;
                    case CLOSE_FRAME:
                        uint16_t close_code = 1000;
                        std::string close_reason = "Shutdown connection";
                        if (payload.size() >= 2) {
                            close_code = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                            if (payload.size() > 2) {
                                close_reason = std::string(payload.begin() + 2, payload.end());
                            }
                        }
                        wait_close_frame_response.store(close_state.load() == CLOSING);
                        end(close_code, close_reason);
                        return;
                }
            } else {
                consume_recv_buffer();
                end(1002, "Protocol error - failed to decode payload");
                return;
            }

            consume_recv_buffer();

            if (close_state == OPEN) {
                asio::async_read(ssl_socket,
                                 recv_buffer,
                                 asio::transfer_at_least(1),
                                 [&](const asio::error_code &ec, const size_t bytes_received) {
                                     read_cb(ec, bytes_received);
                                 });
            }
        }
    };
#endif
}
