#pragma once

#include "ip/net/common.hpp"
#include "ip/utils/dataframe.hpp"
#include "ip/utils/handshake.hpp"

using namespace asio::ip;

namespace internetprotocol {
    struct ws_client_t {
        ws_client_t(): socket(context), resolver(context) {
        }

        asio::io_context context;
        tcp::socket socket;
        tcp::endpoint endpoint;
        tcp::resolver resolver;
    };

    class ws_client_c {
    public:
        ws_client_c() {
            handshake.path = "/chat";
            handshake.headers.insert_or_assign("Connection", "Upgrade");
            handshake.headers.insert_or_assign("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
            //handshake.headers.insert_or_assign("Sec-WebSocket-Protocol", "chat, superchat");
            handshake.headers.insert_or_assign("Sec-WebSocket-Version", "13");
            handshake.headers.insert_or_assign("Upgrade", "websocket");
        }
        ~ws_client_c() {}

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return net.socket.is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * asio::ip::tcp::endpoint ep = client.local_endpoint();;
         * @endcode
         */
        tcp::endpoint local_endpoint() const { return net.socket.local_endpoint(); }

        /**
         * Get the remote endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * asio::ip::tcp::endpoint ep = client.remote_endpoint();
         * @endcode
         */
        tcp::endpoint remote_endpoint() const { return net.socket.remote_endpoint(); }

        /**
         * Return a const ref of the latest error code returned by asio.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * asio::error_code ec = client.get_error_code();
         * @endcode
         */
        const asio::error_code &get_error_code() const { return error_code; }

        /**
         * Return a ref of handshake headers for websocket connection.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * std::map<std::string, std::string> &headers = client.handshake_headers();
         * @endcode
         */
        std::map<std::string, std::string> &handshake_headers() { return handshake.headers; }

        /*MESSAGE*/
        bool write(const std::string &message, const dataframe_t &dataframe = {}) {
            if (!net.socket.is_open() || message.empty()) return false;

            std::string payload = encode_string_payload(message, dataframe);
            asio::async_write(net.socket,
                                asio::buffer(payload.data(), payload.size()),
                                [&](const asio::error_code &ec, const size_t bytes_sent) {
                                    write_cb(ec, bytes_sent);
                                });
            return true;
        }

        bool write_buffer(const std::vector<uint8_t> &buffer, const dataframe_t &dataframe = {}) {
            if (net.socket.is_open() || buffer.empty()) return false;

            std::vector<uint8_t> payload = encode_buffer_payload(buffer, dataframe);
            asio::async_write(net.socket,
                                asio::buffer(payload.data(), payload.size()),
                                [&](const asio::error_code &ec, const size_t bytes_sent) {
                                    write_cb(ec, bytes_sent);
                                });
            return true;
        }

        bool ping() {
            if (!net.socket.is_open()) return false;

            dataframe_t dataframe;
            dataframe.opcode = PING;
            std::vector<uint8_t> payload = encode_buffer_payload({}, dataframe);
            asio::async_write(net.socket,
                                asio::buffer(payload.data(), payload.size()),
                                [&](const asio::error_code &ec, const size_t bytes_sent) {
                                    write_cb(ec, bytes_sent);
                                });
            return true;
        }

        bool pong() {
            if (!net.socket.is_open()) return false;

            dataframe_t dataframe;
            dataframe.opcode = PONG;
            std::vector<uint8_t> payload = encode_buffer_payload({}, dataframe);
            asio::async_write(net.socket,
                                asio::buffer(payload.data(), payload.size()),
                                [&](const asio::error_code &ec, const size_t bytes_sent) {
                                    write_cb(ec, bytes_sent);
                                });
            return true;
        }

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

        void close(const uint16_t code = 0, const std::string &reason = "") {
            is_closing.store(true);
            if (code != 0) {
                dataframe_t dataframe;
                dataframe.opcode = CLOSE_FRAME;
                std::vector<uint8_t> close_payload;
                if (!reason.empty()) {
                    close_payload.insert(close_payload.end(), reason.begin(), reason.end());
                }

                close_payload.push_back(static_cast<uint8_t>(code >> 8));
                close_payload.push_back(static_cast<uint8_t>(code & 0xFF));

                std::vector<uint8_t> encoded_payload = encode_buffer_payload(close_payload, dataframe);
                asio::async_write(net.socket,
                      asio::buffer(encoded_payload.data(), encoded_payload.size()),
                      [&](const asio::error_code &ec, const size_t bytes_sent) {
                          write_cb(ec, bytes_sent, true, code, reason);
                      });
                return;
            }

            close_socket();
            if (on_close) on_close(code, reason);
            is_closing.store(false);
        }

        /**
         * Adds the listener function to 'on_connected'.
         * This event will be triggered after handshake validation.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * client.on_unexpected_handshake = [&](const http_response_t &handshake) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const http_response_t &)> on_connected;

        /**
         * Adds the listener function to 'on_unexpected_handshake'.
         * This event will be triggered if server send an unexpected handshake. Connection will be closed.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * client.on_unexpected_handshake = [&](const http_response_t &handshake) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const http_response_t &)> on_unexpected_handshake;

        /**
         * Adds the listener function to 'on_message_sent'.
         * This event will be triggered when a message has been sent.
         * 'error_code' may be used to check if socket fail to send message.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * client.on_message_sent = [&](const asio::error_code &ec, const size_t bytes_sent, const tcp::endpoint &endpoint) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &, const size_t)> on_message_sent;

        /**
         * Adds the listener function to 'on_message_received'.
         * This event will be triggered when a text message or binary message has been received.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * client.on_message_received = [&](const std::vector<uint8_t> &buffer, bool is_binary) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const std::vector<uint8_t> &, bool)> on_message_received;

        /**
         * Adds the listener function to 'on_ping'.
         * This event will be triggered when a ping message has been received.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * client.on_ping = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_ping;

        /**
         * Adds the listener function to 'on_pong'.
         * This event will be triggered when a pong message has been received.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * client.on_pong = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_pong;

        /**
        * Adds the listener function to 'on_close'.
        * This event will be triggered when a close frame has been received or socket is closed.
        *
        * @par Example
        * @code
        * ws_client_c client;
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
         * ws_client_c client;
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
        ws_client_t net;
        asio::error_code error_code;
        asio::streambuf recv_buffer;
        http_request_t handshake;

        void close_socket() {
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
        }

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
                if (on_error) on_error(error);
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
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }

            std::string request = prepare_request(handshake, net.socket.remote_endpoint().address().to_string(), net.socket.remote_endpoint().port());

            asio::async_write(net.socket, asio::buffer(request.data(), request.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_handshake_cb(ec, bytes_sent);
                              });
        }

        void write_handshake_cb(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                if (!is_closing.load())
                    close();
                return;
            }

            asio::async_read_until(net.socket,
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
                if (!is_closing.load())
                    close();
                return;
            }

            http_response_t response;
            std::istream response_stream(&recv_buffer);
            std::string http_version;
            unsigned int status_code;
            std::string status_message;
            response_stream >> http_version;
            response_stream >> status_code;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                consume_recv_buffer();
                response.status_code = 505;
                response.status_message = "HTTP Version Not Supported";
                if (on_unexpected_handshake) on_unexpected_handshake(response);
                if (!is_closing.load())
                    close(1002, "Protocol error");
                return;
            }
            response.status_code = status_code;
            response.status_message = status_message;
            if (status_code != 101 && recv_buffer.size() == 0) {
                consume_recv_buffer();
                if (on_unexpected_handshake) on_unexpected_handshake(response);
                if (!is_closing.load())
                    close(1002, "Protocol error");
                return;
            }

            asio::async_read_until(net.socket,
                                recv_buffer, "\r\n\r\n",
                                std::bind(&ws_client_c::read_headers, this, asio::placeholders::error, response));
        }

        void read_headers(const asio::error_code &error, http_response_t &response) {
            if (error) {
                consume_recv_buffer();
                if (!is_closing.load())
                    close();
                return;
            }
            std::istream response_stream(&recv_buffer);
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                res_append_header(response, header);

            consume_recv_buffer();

            if (!validate_handshake_response(handshake, response)) {
                if (on_unexpected_handshake) on_unexpected_handshake(response);
                if (!is_closing.load())
                    close(1002, "Protocol error");
                return;
            }

            asio::async_read(net.socket,
                                recv_buffer,
                                asio::transfer_at_least(1),
                                [&](const asio::error_code &ec, const size_t bytes_recvd) {
                                    read_cb(ec, bytes_recvd);
                                });
        }

        void write_cb(const asio::error_code &error, const size_t bytes_sent, const bool close = false, const uint16_t code = 0, const std::string &reason = "") {
            if (error) {
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                if (close) {
                    close_socket();
                    is_closing.store(false);
                    if (on_close) on_close(code, reason);
                }
                return;
            }

            if (close) {
                close_socket();
                is_closing.store(false);
                if (on_close) on_close(code, reason);
                return;
            }

            if (on_message_sent) on_message_sent(error, bytes_sent);
        }

        void consume_recv_buffer() {
            const size_t size = recv_buffer.size();
            if (size > 0)
                recv_buffer.consume(size);
        }

        void read_cb(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                std::lock_guard lock(mutex_error);
                consume_recv_buffer();
                if (on_error) on_error(error);
                if (!is_closing.load())
                    close();
                return;
            }

            std::vector<uint8_t> buffer;
            buffer.resize(bytes_recvd);
            asio::buffer_copy(asio::buffer(buffer, bytes_recvd),
                                recv_buffer.data());

            dataframe_t dataframe;
            std::vector<uint8_t> payload{};
            if (decode_payload(buffer, payload, dataframe)) {
                switch (dataframe.opcode) {
                    case TEXT_FRAME:
                        if (on_message_received) on_message_received(payload, false);
                        break;
                    case BINARY_FRAME:
                        if (on_message_received) on_message_received(payload, true);
                        break;
                    case PING:
                        if (on_ping) on_ping();
                        break;
                    case PONG:
                        if (on_pong) on_pong();
                        break;
                    case CLOSE_FRAME:
                        uint16_t close_code = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                        std::string close_reason = payload.size() > 2 ? std::string(payload.begin() + 2, payload.end()) : "";
                        close(close_code, close_reason);
                        break;
                }
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
}