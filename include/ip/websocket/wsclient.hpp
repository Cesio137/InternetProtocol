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
            handshake.headers.insert_or_assign("Sec-WebSocket-Version", "13");
            handshake.headers.insert_or_assign("Upgrade", "websocket");
        }
        ~ws_client_c() {
            if (net.socket.is_open())
                close(1000, "Normal closure");
        }

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return net.socket.is_open() && close_state == OPEN; }

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
         * Return a ref of handshake headers.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * std::map<std::string, std::string> &headers = client.handshake_headers();
         * @endcode
         */
        std::map<std::string, std::string> &handshake_headers() { return handshake.headers; }

        /**
         * Sends string data on the socket.
         * It returns false if socket is closed or if buffer is empty.
         *
         * @param message String to be send.
         *
         * @param dataframe Custom dataframe if needed.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * client.connect("localhost", 8080, v4);
         *
         * std::string message = "...";
         * client.write(message);
         * @endcode
         */
        bool write(const std::string &message, const dataframe_t &dataframe = {}) {
            if (!is_open() || message.empty()) return false;

            dataframe_t frame = dataframe;
            frame.opcode = TEXT_FRAME;
            std::string payload = encode_string_payload(message, frame);
            asio::async_write(net.socket,
                                asio::buffer(payload.data(), payload.size()),
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
         * @param dataframe Custom dataframe if needed.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * client.connect("localhost", 8080, v4);
         *
         * std::vector<uint8_t> buffer = { 0, 1, 2, ... };
         * client.write_buffer(buffer);
         * @endcode
         */
        bool write_buffer(const std::vector<uint8_t> &buffer, const dataframe_t &dataframe = {}) {
            if (!is_open() || buffer.empty()) return false;

            dataframe_t frame = dataframe;
            frame.opcode = BINARY_FRAME;
            std::vector<uint8_t> payload = encode_buffer_payload(buffer, frame);
            asio::async_write(net.socket,
                                asio::buffer(payload.data(), payload.size()),
                                [&](const asio::error_code &ec, const size_t bytes_sent) {
                                    write_cb(ec, bytes_sent);
                                });
            return true;
        }

        /**
         * Send a ping message.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * client.ping();
         * @endcode
         */
        bool ping() {
            if (!is_open()) return false;

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

        /**
         * Send a pong message. This is already performed automatically.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * client.pong();
         * @endcode
         */
        bool pong() {
            if (!is_open()) return false;

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
         * ws_client_c client;
         * client.connect("localhost", 8080, v4, true);
         * @endcode
         */
        bool connect(const std::string &address = "localhost", const std::string &port = "8080", const protocol_type_e protocol = v4) {
            if (net.socket.is_open())
                return false;

            close_state = OPEN;
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
         * @param code Closing code.
         *
         * @param reason Reason.
         *
         * @par Example
         * @code
         * ws_client_c client;
         * client.close(1000, "Finished");
         * @endcode
         */
        void close(const uint16_t code = 1000, const std::string &reason = "") {
            if (close_state == CLOSED) return;
            
            if (close_state == OPEN) {
                // Client send close frame
                close_state = CLOSING;
                send_close_frame(code, reason);
            } else {
                // End connection
                perform_close(code, reason);
            }
        }

        /**
         * Adds the listener function to 'on_connected'.
         * This event will be triggered when socket start to listening.
         *
         * @par Example
         * @code
         * ws_client_c client();
         * client.on_connected = [&](const http_response_t &server_handshake) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const http_response_t &)> on_connected;

        /**
         * Adds the listener function to 'on_unexpected_handshake'.
         * This event will be triggered when a bad handshake has been received from server.
         *
         * @par Example
         * @code
         * ws_client_c client();
         * client.on_unexpected_handshake = [&](const http_response_t &server_handshake) {
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
         * ws_client_c client();
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
         * ws_client_c client();
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
         * ws_client_c client();
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
         * ws_client_c client();
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
         * ws_client_c client();
         * client.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_error;
        close_state_e close_state = CLOSED;
        ws_client_t net;
        asio::error_code error_code;
        asio::streambuf recv_buffer;
        http_request_t handshake;

        // Send close frame
        void send_close_frame(const uint16_t code, const std::string &reason) {
            if (!net.socket.is_open()) {
                perform_close(code, reason);
                return;
            }

            dataframe_t dataframe;
            dataframe.opcode = CLOSE_FRAME;
            
            std::vector<uint8_t> close_payload;
            // status code
            close_payload.push_back(static_cast<uint8_t>(code >> 8));
            close_payload.push_back(static_cast<uint8_t>(code & 0xFF));
            
            // Reason
            if (!reason.empty()) {
                close_payload.insert(close_payload.end(), reason.begin(), reason.end());
            }

            std::vector<uint8_t> encoded_payload = encode_buffer_payload(close_payload, dataframe);
            asio::async_write(net.socket,
                              asio::buffer(encoded_payload.data(), encoded_payload.size()),
                              [this, code, reason](const asio::error_code &ec, const size_t bytes_sent) {
                                  close_frame_sent_cb(ec, bytes_sent, code, reason);
                              });
        }

        // Callback for close frame
        void close_frame_sent_cb(const asio::error_code &error, const size_t bytes_sent, 
                                const uint16_t code, const std::string &reason) {
            if (error) {
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                perform_close(code, reason);
            }

            // Close frame has benn sent
        }

        // Handle close frame received
        void handle_close_frame_received(const uint16_t code, const std::string &reason) {
            if (close_state == OPEN) {
                close_state = CLOSING;
                send_close_frame(code, reason);
                perform_close(code, reason);
                
            } else if (close_state == CLOSING) {
                perform_close(code, reason);
            }
        }

        void perform_close(const uint16_t code, const std::string &reason) {
            if (close_state == CLOSED) return;
            
            close_state = CLOSED;
            close_socket();
            
            if (on_close) {
                on_close(code, reason);
            }
        }

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
            if (close_state == OPEN) {
                close(1006, "Abnormal closure");
            }
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
                close(1002, "Protocol error");
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
                close(1002, "Protocol error");
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
                close(1002, "Protocol error");
                return;
            }
            response.status_code = status_code;
            response.status_message = status_message;
            if (status_code != 101 && recv_buffer.size() == 0) {
                consume_recv_buffer();
                if (on_unexpected_handshake) on_unexpected_handshake(response);
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
                close(1002, "Protocol error");
                return;
            }
            std::istream response_stream(&recv_buffer);
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                res_append_header(response, header);

            consume_recv_buffer();

            if (!validate_handshake_response(handshake, response)) {
                if (on_unexpected_handshake) on_unexpected_handshake(response);
                close(1002, "Protocol error");
                return;
            }

            if (on_connected) on_connected(response);

            asio::async_read(net.socket,
                                recv_buffer,
                                asio::transfer_at_least(1),
                                [&](const asio::error_code &ec, const size_t bytes_recvd) {
                                    read_cb(ec, bytes_recvd);
                                });
        }

        void write_cb(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
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
                close(1006, "Abnormal closure");
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
                        pong();
                        break;
                    case PONG:
                        if (on_pong) on_pong();
                        break;
                    case CLOSE_FRAME:
                        uint16_t close_code = 1000;
                        std::string close_reason = "";
                        
                        if (payload.size() >= 2) {
                            close_code = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                            if (payload.size() > 2) {
                                close_reason = std::string(payload.begin() + 2, payload.end());
                            }
                        }
                        
                        handle_close_frame_received(close_code, close_reason);
                        return;
                }
            }

            consume_recv_buffer();

            if (close_state == OPEN) {
                asio::async_read(net.socket,
                                    recv_buffer,
                                    asio::transfer_at_least(1),
                                    [&](const asio::error_code &ec, const size_t bytes_received) {
                                        read_cb(ec, bytes_received);
                                    });
            }
        }
    };

#ifdef ENABLE_SSL
    struct ws_client_ssl_t {
        ws_client_ssl_t(): ssl_context(asio::ssl::context::tlsv13_client),
                            ssl_socket(context, ssl_context),
                            resolver(context) {
        }

        asio::io_context context;
        asio::ssl::context ssl_context;
        tcp::resolver resolver;
        tcp::endpoint endpoint;
        asio::ssl::stream<tcp::socket> ssl_socket;
    };

    class ws_client_ssl_c {
    public:
        ws_client_ssl_c(const security_context_opts sec_opts = {}) {
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

            handshake.path = "/chat";
            handshake.headers.insert_or_assign("Connection", "Upgrade");
            handshake.headers.insert_or_assign("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
            handshake.headers.insert_or_assign("Sec-WebSocket-Version", "13");
            handshake.headers.insert_or_assign("Upgrade", "websocket");
        }
        ~ws_client_ssl_c() {}

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * ws_client_ssl_c client({});
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return net.ssl_socket.next_layer().is_open() && close_state == OPEN; }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * ws_client_ssl_c client({});
         * asio::ip::tcp::endpoint ep = client.local_endpoint();;
         * @endcode
         */
        tcp::endpoint local_endpoint() const { return net.ssl_socket.next_layer().local_endpoint(); }

        /**
         * Get the remote endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * ws_client_ssl_c client({});
         * asio::ip::tcp::endpoint ep = client.remote_endpoint();
         * @endcode
         */
        tcp::endpoint remote_endpoint() const { return net.ssl_socket.next_layer().remote_endpoint(); }

        /**
         * Return a const ref of the latest error code returned by asio.
         *
         * @par Example
         * @code
         * ws_client_ssl_c client({});
         * asio::error_code ec = client.get_error_code();
         * @endcode
         */
        const asio::error_code &get_error_code() const { return error_code; }

        /**
         * Return a ref of handshake headers.
         *
         * @par Example
         * @code
         * ws_client_ssl_c client({});
         * std::map<std::string, std::string> &headers = client.handshake_headers();
         * @endcode
         */
        std::map<std::string, std::string> &handshake_headers() { return handshake.headers; }

        bool write(const std::string &message, const dataframe_t &dataframe = {}) {
            if (!is_open() || message.empty()) return false;

            std::string payload = encode_string_payload(message, dataframe);
            asio::async_write(net.ssl_socket,
                                asio::buffer(payload.data(), payload.size()),
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
         * @param dataframe Custom dataframe if needed.
         *
         * @par Example
         * @code
         * ws_client_ssl_c client({});
         * client.connect("localhost", 8080, v4);
         *
         * std::vector<uint8_t> buffer = { 0, 1, 2, ... };
         * client.write_buffer(buffer);
         * @endcode
         */
        bool write_buffer(const std::vector<uint8_t> &buffer, const dataframe_t &dataframe = {}) {
            if (!is_open() || buffer.empty()) return false;

            std::vector<uint8_t> payload = encode_buffer_payload(buffer, dataframe);
            asio::async_write(net.ssl_socket,
                                asio::buffer(payload.data(), payload.size()),
                                [&](const asio::error_code &ec, const size_t bytes_sent) {
                                    write_cb(ec, bytes_sent);
                                });
            return true;
        }

        /**
         * Send a ping message.
         *
         * @par Example
         * @code
         * ws_client_ssl_c client({});
         * client.ping();
         * @endcode
         */
        bool ping() {
            if (!is_open()) return false;

            dataframe_t dataframe;
            dataframe.opcode = PING;
            std::vector<uint8_t> payload = encode_buffer_payload({}, dataframe);
            asio::async_write(net.ssl_socket,
                                asio::buffer(payload.data(), payload.size()),
                                [&](const asio::error_code &ec, const size_t bytes_sent) {
                                    write_cb(ec, bytes_sent);
                                });
            return true;
        }

        /**
         * Send a pong message. This is already performed automatically.
         *
         * @par Example
         * @code
         * ws_client_ssl_c client({});
         * client.pong();
         * @endcode
         */
        bool pong() {
            if (!is_open()) return false;

            dataframe_t dataframe;
            dataframe.opcode = PONG;
            std::vector<uint8_t> payload = encode_buffer_payload({}, dataframe);
            asio::async_write(net.ssl_socket,
                                asio::buffer(payload.data(), payload.size()),
                                [&](const asio::error_code &ec, const size_t bytes_sent) {
                                    write_cb(ec, bytes_sent);
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
         * ws_client_ssl_c client({});
         * client.connect("localhost", 8080, v4, true);
         * @endcode
         */
        bool connect(const std::string &address = "localhost", const std::string &port = "8080", const protocol_type_e protocol = v4) {
            if (net.ssl_socket.next_layer().is_open())
                return false;

            close_state = OPEN;
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
         * @param code Closing code.
         *
         * @param reason Reaspn.
         *
         * @par Example
         * @code
         * ws_client_ssl_c client({});
         * client.close(1000, "Finished");
         * @endcode
         */
        void close(const uint16_t code = 1000, const std::string &reason = "") {
            if (close_state == CLOSED) return;

            if (close_state == OPEN) {
                // Client send close frame
                close_state = CLOSING;
                send_close_frame(code, reason);
            } else {
                // End connection
                perform_close(code, reason);
            }
        }

        /**
         * Adds the listener function to 'on_connected'.
         * This event will be triggered when socket start to listening.
         *
         * @par Example
         * @code
         * ws_client_ssl_c client({});
         * client.on_connected = [&](const http_response_t &server_handshake) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const http_response_t &)> on_connected;

        /**
         * Adds the listener function to 'on_unexpected_handshake'.
         * This event will be triggered when a bad handshake has been received from server.
         *
         * @par Example
         * @code
         * ws_client_ssl_c client({});
         * client.on_unexpected_handshake = [&](const http_response_t &server_handshake) {
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
         * ws_client_ssl_c client({});
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
         * ws_client_ssl_c client({});
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
         * ws_client_ssl_c client({});
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
         * ws_client_ssl_c client({});
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
         * tcp_client_ssl_c client({});
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
         * ws_client_ssl_c client({});
         * client.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_error;
        close_state_e close_state = CLOSED;
        ws_client_ssl_t net;
        asio::error_code error_code;
        asio::streambuf recv_buffer;
        http_request_t handshake;

        // Send close frame
        void send_close_frame(const uint16_t code, const std::string &reason) {
            if (!net.ssl_socket.next_layer().is_open()) {
                perform_close(code, reason);
                return;
            }

            dataframe_t dataframe;
            dataframe.opcode = CLOSE_FRAME;

            std::vector<uint8_t> close_payload;
            // status code
            close_payload.push_back(static_cast<uint8_t>(code >> 8));
            close_payload.push_back(static_cast<uint8_t>(code & 0xFF));

            // Reason
            if (!reason.empty()) {
                close_payload.insert(close_payload.end(), reason.begin(), reason.end());
            }

            std::vector<uint8_t> encoded_payload = encode_buffer_payload(close_payload, dataframe);
            asio::async_write(net.ssl_socket,
                              asio::buffer(encoded_payload.data(), encoded_payload.size()),
                              [this, code, reason](const asio::error_code &ec, const size_t bytes_sent) {
                                  close_frame_sent_cb(ec, bytes_sent, code, reason);
                              });
        }

        // Callback for close frame
        void close_frame_sent_cb(const asio::error_code &error, const size_t bytes_sent,
                                const uint16_t code, const std::string &reason) {
            if (error) {
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                perform_close(code, reason);
            }

            // Close frame has been sent
        }

        // Handle close frame received
        void handle_close_frame_received(const uint16_t code, const std::string &reason) {
            if (close_state == OPEN) {
                close_state = CLOSING;
                send_close_frame(code, reason);
                perform_close(code, reason);

            } else if (close_state == CLOSING) {
                perform_close(code, reason);
            }
        }

        void perform_close(const uint16_t code, const std::string &reason) {
            if (close_state == CLOSED) return;

            close_state = CLOSED;
            close_socket();

            if (on_close) {
                on_close(code, reason);
            }
        }

        void close_socket() {
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
        }

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            error_code.clear();
            net.context.run();
            if (close_state == OPEN) {
                close(1006, "Abnormal closure");
            }
        }

        void resolve(const asio::error_code &error, const tcp::resolver::results_type &results) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
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
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }

            net.ssl_socket.async_handshake(asio::ssl::stream_base::client,
                                            [&](const asio::error_code &ec) {
                                                ssl_handshake(ec);
                                            });
        }

        void ssl_handshake(const asio::error_code &error) {
            if (error) {
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }

            std::string request = prepare_request(handshake, net.ssl_socket.next_layer().remote_endpoint().address().to_string(), net.ssl_socket.next_layer().remote_endpoint().port());

            asio::async_write(net.ssl_socket, asio::buffer(request.data(), request.size()),
                              [&](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_handshake_cb(ec, bytes_sent);
                              });
        }

        void write_handshake_cb(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                close(1002, "Protocol error");
                return;
            }

            asio::async_read_until(net.ssl_socket,
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
                close(1002, "Protocol error");
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
                close(1002, "Protocol error");
                return;
            }
            response.status_code = status_code;
            response.status_message = status_message;
            if (status_code != 101 && recv_buffer.size() == 0) {
                consume_recv_buffer();
                if (on_unexpected_handshake) on_unexpected_handshake(response);
                close(1002, "Protocol error");
                return;
            }

            asio::async_read_until(net.ssl_socket,
                                recv_buffer, "\r\n\r\n",
                                std::bind(&ws_client_ssl_c::read_headers, this, asio::placeholders::error, response));
        }

        void read_headers(const asio::error_code &error, http_response_t &response) {
            if (error) {
                consume_recv_buffer();
                close(1002, "Protocol error");
                return;
            }
            std::istream response_stream(&recv_buffer);
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                res_append_header(response, header);

            consume_recv_buffer();

            if (!validate_handshake_response(handshake, response)) {
                if (on_unexpected_handshake) on_unexpected_handshake(response);
                close(1002, "Protocol error");
                return;
            }

            if (on_connected) on_connected(response);

            asio::async_read(net.ssl_socket,
                                recv_buffer,
                                asio::transfer_at_least(1),
                                [&](const asio::error_code &ec, const size_t bytes_recvd) {
                                    read_cb(ec, bytes_recvd);
                                });
        }

        void write_cb(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                std::lock_guard lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
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
                close(1006, "Abnormal closure");
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
                        pong();
                        break;
                    case PONG:
                        if (on_pong) on_pong();
                        break;
                    case CLOSE_FRAME:
                        uint16_t close_code = 1000;
                        std::string close_reason = "";

                        if (payload.size() >= 2) {
                            close_code = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                            if (payload.size() > 2) {
                                close_reason = std::string(payload.begin() + 2, payload.end());
                            }
                        }

                        handle_close_frame_received(close_code, close_reason);
                        return;
                }
            }

            consume_recv_buffer();

            if (close_state == OPEN) {
                asio::async_read(net.ssl_socket,
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