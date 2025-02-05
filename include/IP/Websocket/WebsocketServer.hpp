/*
 * Copyright (c) 2023-2025 Nathan Miguel
 *
 * InternetProtocol is free library: you can redistribute it and/or modify it under the terms
 * of the GNU Affero General Public License as published by the Free Software Foundation,
 * version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
*/

#pragma once

#include <IP/Net/Message.hpp>

namespace InternetProtocol {
    class WebsocketServer {
    public:
        WebsocketServer() {
            res_handshake.headers["Connection"] = "Upgrade";
            res_handshake.headers["Sec-WebSocket-Accept"] = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
            res_handshake.headers["Upgrade"] = "websocket";
            sdata_frame.mask = false;
        }

        ~WebsocketServer() {
            if (get_acceptor().is_open()) close();
        }

        /*HOST*/
        void set_socket(const EProtocolType protocol, const uint16_t port,
                        const int max_listen_conn = 2147483647) {
            tcp_protocol = protocol;
            tcp_port = port;
            backlog = max_listen_conn;
        }


        asio::ip::tcp::acceptor &get_acceptor() { return tcp.acceptor; }

        const std::set<socket_ptr> get_sockets() const {
            return tcp.sockets;
        }

        /*SETTINGS*/
        void set_max_send_buffer_size(int value = 1400) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*HANDSHAKE*/
        void append_headers(const std::string &key, const std::string &value) {
            res_handshake.headers.insert_or_assign(key, value);
        }

        void clear_headers() { res_handshake.headers.clear(); }

        void remove_header(const std::string &key) {
            if (!res_handshake.headers.contains(key)) return;
            res_handshake.headers.erase(key);
        }

        bool has_header(const std::string &key) const {
            return res_handshake.headers.contains(key);
        }

        const std::string &get_header(const std::string &key) {
            return res_handshake.headers[key];
        }

        /*DATAFRAME*/
        void set_RSV1(bool value = false) { sdata_frame.rsv1 = value; }
        bool use_RSV1() const { return sdata_frame.rsv1; }
        void set_RSV2(bool value = false) { sdata_frame.rsv2 = value; }
        bool use_RSV2() const { return sdata_frame.rsv2; }
        void set_RSV3(bool value = false) { sdata_frame.rsv3 = value; }
        bool use_RSV3() const { return sdata_frame.rsv3; }

        /*MESSAGE*/
        bool send_handshake_to(const Server::FRequest &request, Server::FResponse &response, const socket_ptr &socket) {
            if (!socket->is_open()) return false;

            asio::post(thread_pool,
                       std::bind(&WebsocketServer::package_handshake, this, request, response, socket, 101));
            return true;
        }

        bool send_handshake_error_to(const uint32_t status_code, const std::string &body, const socket_ptr &socket) {
            if (!socket->is_open()) return false;

            asio::post(thread_pool, std::bind(&WebsocketServer::package_handshake_error, this, status_code, body, socket));
            return true;
        }

        bool send_str_to(const std::string &message, const socket_ptr &socket) {
            if (!socket->is_open() || message.empty()) return false;

            asio::post(thread_pool, std::bind(&WebsocketServer::post_string, this, message, socket));
            return true;
        }

        bool send_buffer_to(const std::vector<uint8_t> &buffer, const socket_ptr &socket) {
            if (!socket->is_open() || buffer.empty()) return false;

            asio::post(thread_pool, std::bind(&WebsocketServer::post_buffer, this,
                                              EOpcode::BINARY_FRAME, buffer, socket));
            return true;
        }

        bool send_ping_to(const socket_ptr &socket) {
            if (!socket->is_open()) return false;

            std::vector<uint8_t> ping_buffer = {'p', 'i', 'n', 'g', '\0'};
            asio::post(thread_pool, std::bind(&WebsocketServer::post_buffer, this,
                                              EOpcode::PING, ping_buffer, socket));
            return true;
        }

        /*CONNECTION*/
        bool open() {
            if (get_acceptor().is_open())
                return false;

            asio::ip::tcp::endpoint endpoint(tcp_protocol == EProtocolType::V4
                                                 ? asio::ip::tcp::v4()
                                                 : asio::ip::tcp::v6(), tcp_port);
            error_code = asio::error_code();
            tcp.acceptor.open(tcp_protocol == EProtocolType::V4
                                  ? asio::ip::tcp::v4()
                                  : asio::ip::tcp::v6(), error_code);
            if (error_code && on_error) {
                on_error(error_code);
                return false;
            }
            tcp.acceptor.set_option(asio::socket_base::reuse_address(true), error_code);
            if (error_code && on_error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }
            tcp.acceptor.bind(endpoint, error_code);
            if (error_code && on_error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }
            tcp.acceptor.listen(backlog, error_code);
            if (error_code && on_error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }

            asio::post(thread_pool, std::bind(&WebsocketServer::run_context_thread, this));
            return true;
        }

        void close() {
            is_closing = true;
            if (!tcp.sockets.empty())
                for (const socket_ptr &socket: tcp.sockets) {
                    if (socket->is_open()) {
                        std::lock_guard<std::mutex> guard(mutex_error);
                        bool has_error = false;
                        socket->shutdown(asio::ip::tcp::socket::shutdown_send, error_code);
                        if (on_socket_disconnected && error_code) {
                            has_error = true;
                            on_socket_disconnected(error_code, socket);
                        }
                        socket->close(error_code);
                        if (on_socket_disconnected && error_code) {
                            has_error = true;
                            on_socket_disconnected(error_code, socket);
                        }
                        if (on_socket_disconnected && !has_error)
                            on_socket_disconnected(error_code, socket);
                    }
                }
            tcp.context.stop();
            if (!tcp.sockets.empty()) tcp.sockets.clear();
            if (!listening_buffers.empty()) listening_buffers.clear();
            if (tcp.acceptor.is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                tcp.acceptor.close(error_code);
                if (on_error) on_error(error_code);
            }
            tcp.context.restart();
            tcp.acceptor = asio::ip::tcp::acceptor(tcp.context);
            if (on_close)
                on_close();
            is_closing = false;
        }

        void disconnect_socket(const socket_ptr &socket) {
            bool has_error = false;
            if (socket->is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                socket->shutdown(asio::ip::tcp::socket::shutdown_send, error_code);
                if (on_socket_disconnected && error_code) {
                    has_error = true;
                    on_socket_disconnected(error_code, socket);
                }
                socket->close(error_code);
                if (on_socket_disconnected && error_code) {
                    has_error = true;
                    on_socket_disconnected(error_code, socket);
                }
            }
            if (listening_buffers.contains(socket))
                listening_buffers.erase(socket);
            if (tcp.sockets.contains(socket))
                tcp.sockets.erase(socket);
            if (on_socket_disconnected && !has_error) on_socket_disconnected(asio::error_code(), socket);
        }

        /*EVENTS*/
        std::function<void(const Server::FRequest, const Server::FResponse, const socket_ptr &)> on_socket_accepted;
        std::function<void(const size_t, const size_t)> on_bytes_transfered;
        std::function<void(const asio::error_code &, const socket_ptr &)> on_message_sent;
        std::function<void(const FWsMessage, const socket_ptr &)> on_message_received;
        std::function<void(const socket_ptr &)> on_pong_received;
        std::function<void(const socket_ptr &)> on_close_notify;
        std::function<void(const asio::error_code &, const socket_ptr &)> on_socket_disconnected;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        std::mutex mutex_error;
        bool is_closing = false;
        Server::FAsioTcp tcp;
        asio::error_code error_code;
        EProtocolType tcp_protocol = EProtocolType::V4;
        uint16_t tcp_port = 3000;
        int backlog = 2147483647;
        bool split_buffer = false;
        int max_send_buffer_size = 1400;
        Server::FRequest req_handshake;
        Server::FResponse res_handshake;
        std::map<socket_ptr, std::shared_ptr<asio::streambuf> > listening_buffers;
        FDataFrame sdata_frame;

        void disconnect_socket_after_error(const asio::error_code &error, const socket_ptr &socket) {
            if (socket->is_open()) {
                std::lock_guard<std::mutex> guard(mutex_buffer);
                socket->shutdown(asio::ip::tcp::socket::shutdown_both, error_code);
                if (on_socket_disconnected && error_code) {
                    on_socket_disconnected(error_code, socket);
                }
                socket->close(error_code);
                if (on_socket_disconnected && error_code) {
                    on_socket_disconnected(error_code, socket);
                }
            }
            if (listening_buffers.contains(socket)) {
                listening_buffers.erase(socket);
            }
            if (tcp.sockets.contains(socket)) {
                tcp.sockets.erase(socket);
            }
            if (on_socket_disconnected) on_socket_disconnected(error, socket);
        }

        void post_string(const std::string &str, const socket_ptr &socket) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            sdata_frame.opcode = EOpcode::TEXT_FRAME;
            package_string(str, socket);
        }

        void post_buffer(EOpcode opcode, const std::vector<uint8_t> &buffer, const socket_ptr &socket) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            sdata_frame.opcode = opcode;
            if (opcode == EOpcode::BINARY_FRAME) {
                package_buffer(buffer, socket);
            } else if (opcode == EOpcode::PING || opcode == EOpcode::PONG) {
                std::vector<uint8_t> p_buffer = buffer;
                encode_buffer_payload(p_buffer);
                asio::async_write(
                    *socket, asio::buffer(p_buffer.data(), p_buffer.size()),
                    std::bind(&WebsocketServer::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, socket));
            }
        }

        void package_string(const std::string &str, const socket_ptr &socket) {
            std::string payload;
            if (!split_buffer ||
                str.size() + get_frame_encode_size(str.size()) <= max_send_buffer_size) {
                sdata_frame.fin = true;
                payload = encode_string_payload(str);
                asio::async_write(
                    *socket, asio::buffer(payload.data(), payload.size()),
                    std::bind(&WebsocketServer::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, socket));
                return;
            }

            sdata_frame.fin = true;
            size_t string_offset = 0;
            const size_t max_size =
                    max_send_buffer_size - get_frame_encode_size(str.size());
            while (string_offset < str.size()) {
                sdata_frame.fin = string_offset < str.size();
                size_t package_size = std::min(max_size, str.size() - string_offset);
                payload.assign(str.begin() + string_offset,
                               str.begin() + string_offset + package_size);
                payload = encode_string_payload(payload);
                asio::async_write(
                    *socket, asio::buffer(payload, payload.size()),
                    std::bind(&WebsocketServer::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, socket));
                string_offset += package_size;
                if (sdata_frame.opcode != EOpcode::FRAME_CON)
                    sdata_frame.opcode = EOpcode::FRAME_CON;
            }
        }

        std::string encode_string_payload(const std::string &payload) {
            std::string string_buffer;
            uint64_t payload_length = payload.size();

            // FIN, RSV, Opcode
            uint8_t byte1 = sdata_frame.fin ? 0x80 : 0x00;
            byte1 |= sdata_frame.rsv1 ? (uint8_t) ERSV::RSV1 : 0x00;
            byte1 |= sdata_frame.rsv2 ? (uint8_t) ERSV::RSV2 : 0x00;
            byte1 |= sdata_frame.rsv3 ? (uint8_t) ERSV::RSV3 : 0x00;
            byte1 |= (uint8_t) sdata_frame.opcode & 0x0F;
            string_buffer.push_back(byte1);

            // Mask and payload size
            uint8_t byte2 = sdata_frame.mask ? 0x80 : 0x00;
            if (payload_length <= 125) {
                byte2 |= payload_length;
                string_buffer.push_back(byte2);
            } else if (payload_length <= 65535) {
                byte2 |= 126;
                string_buffer.push_back(byte2);
                string_buffer.push_back((payload_length >> 8) & 0xFF);
                string_buffer.push_back(payload_length & 0xFF);
            } else {
                byte2 |= 127;
                string_buffer.push_back(byte2);
                for (int i = 7; i >= 0; --i) {
                    string_buffer.push_back((payload_length >> (8 * i)) & 0xFF);
                }
            }

            std::array<uint8_t, 4> masking_key;
            if (sdata_frame.mask) {
                masking_key = mask_gen();
                for (uint8_t key: masking_key)
                    string_buffer.push_back(static_cast<uint8_t>(key));
            }

            // payload data and mask
            for (size_t i = 0; i < payload.size(); ++i) {
                if (sdata_frame.mask) {
                    string_buffer.push_back(payload[i] ^
                                            static_cast<uint8_t>(masking_key[i % 4]));
                } else {
                    string_buffer.push_back(payload[i]);
                }
            }

            return string_buffer;
        }

        void package_buffer(const std::vector<uint8_t> &buffer, const socket_ptr &socket) {
            std::vector<uint8_t> payload;
            if (!split_buffer || buffer.size() + get_frame_encode_size(buffer.size()) <=
                max_send_buffer_size) {
                sdata_frame.fin = true;
                payload = encode_buffer_payload(buffer);
                asio::async_write(
                    *socket, asio::buffer(payload.data(), payload.size()),
                    std::bind(&WebsocketServer::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, socket));
                return;
            }

            sdata_frame.fin = false;
            size_t buffer_offset = 0;
            const size_t max_size =
                    max_send_buffer_size - get_frame_encode_size(buffer.size());
            while (buffer_offset < buffer.size()) {
                sdata_frame.fin = buffer_offset < buffer.size();
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                payload.assign(buffer.begin() + buffer_offset,
                               buffer.begin() + buffer_offset + package_size);
                encode_buffer_payload(payload);
                asio::async_write(
                    *socket, asio::buffer(payload, payload.size()),
                    std::bind(&WebsocketServer::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, socket));
                buffer_offset += package_size;
                if (sdata_frame.opcode != EOpcode::FRAME_CON)
                    sdata_frame.opcode = EOpcode::FRAME_CON;
            }
        }

        std::vector<uint8_t> encode_buffer_payload(
            const std::vector<uint8_t> &payload) {
            std::vector<uint8_t> buffer;
            uint64_t payload_length = payload.size();

            // FIN, RSV, Opcode
            uint8_t byte1 = uint8_t(sdata_frame.fin ? 0x80 : 0x00);
            byte1 |= uint8_t(sdata_frame.rsv1 ? (uint8_t) ERSV::RSV1 : 0x00);
            byte1 |= uint8_t(sdata_frame.rsv2 ? (uint8_t) ERSV::RSV2 : 0x00);
            byte1 |= uint8_t(sdata_frame.rsv3 ? (uint8_t) ERSV::RSV3 : 0x00);
            byte1 |= uint8_t((uint8_t) sdata_frame.opcode & 0x0F);
            buffer.push_back(byte1);

            // Mask and payload size
            uint8_t byte2 = uint8_t(sdata_frame.mask ? 0x80 : 0x00);
            if (payload_length <= 125) {
                byte2 |= uint8_t(payload_length);
                buffer.push_back(byte2);
            } else if (payload_length <= 65535) {
                byte2 |= uint8_t(126);
                buffer.push_back(byte2);
                buffer.push_back(uint8_t((payload_length >> 8) & 0xFF));
                buffer.push_back(uint8_t(payload_length & 0xFF));
            } else {
                byte2 |= uint8_t(127);
                buffer.push_back(byte2);
                for (int i = 7; i >= 0; --i) {
                    buffer.push_back(uint8_t((payload_length >> (8 * i)) & 0xFF));
                }
            }

            std::array<uint8_t, 4> masking_key;
            if (sdata_frame.mask) {
                masking_key = mask_gen();
                for (uint8_t key: masking_key) buffer.push_back(key);
            }

            // payload data and mask
            for (size_t i = 0; i < payload.size(); ++i) {
                if (sdata_frame.mask) {
                    buffer.push_back(payload[i] ^ masking_key[i % 4]);
                } else {
                    buffer.push_back(payload[i]);
                }
            }

            return buffer;
        }

        size_t get_frame_encode_size(const size_t buffer_size) {
            size_t size = 2;
            if (buffer_size <= 125) {
                size += 0;
            } else if (buffer_size <= 65535) {
                size += 2;
            } else {
                size += 8;
            }

            if (sdata_frame.mask) size += 4;

            return size;
        }

        std::array<uint8_t, 4> mask_gen() {
            std::array<uint8_t, 4> maskKey;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);

            for (uint8_t &byte: maskKey) {
                byte = uint8_t(dis(gen));
            }

            return maskKey;
        }

        bool decode_payload(FWsMessage &data_frame, const socket_ptr &socket) {
            if (asio::buffer_size(listening_buffers.at(socket)->data()) < 2) return false;

            size_t size = asio::buffer_size(listening_buffers.at(socket)->data());
            std::vector<uint8_t> encoded_buffer;
            encoded_buffer.resize(size);
            asio::buffer_copy(asio::buffer(encoded_buffer, encoded_buffer.size()),
                              listening_buffers.at(socket)->data());

            size_t pos = 0;
            // FIN, RSV, Opcode
            uint8_t byte1 = encoded_buffer[pos++];
            data_frame.data_frame.fin = (uint8_t) byte1 & 0x80;
            data_frame.data_frame.rsv1 = (uint8_t) byte1 & 0x80;
            data_frame.data_frame.rsv2 = (uint8_t) byte1 & 0x40;
            data_frame.data_frame.rsv3 = (uint8_t) byte1 & 0x10;
            data_frame.data_frame.opcode = (EOpcode) ((uint8_t) byte1 & 0x0F);

            // Mask and payload length
            uint8_t byte2 = encoded_buffer[pos++];
            data_frame.data_frame.mask = (uint8_t) byte2 & 0x80;
            uint64_t payload_length = (uint8_t) byte2 & 0x7F;
            if (payload_length == 126) {
                if (encoded_buffer.size() < pos + 2) return false;
                payload_length = static_cast<uint64_t>((encoded_buffer[pos] << 8) |
                                                       encoded_buffer[pos + 1]);
                pos += 2;
            } else if (payload_length == 127) {
                if (encoded_buffer.size() < pos + 8) return false;
                payload_length = 0;
                for (int i = 0; i < 8; ++i) {
                    payload_length = static_cast<uint64_t>(
                        (uint8_t(payload_length) << 8) | encoded_buffer[pos + i]);
                }
                pos += 8;
            }
            data_frame.data_frame.length = payload_length;

            // Masking key
            if (data_frame.data_frame.mask) {
                if (encoded_buffer.size() < pos + 4) return false;
                for (int i = 0; i < 4; ++i) {
                    data_frame.data_frame.masking_key[i] = encoded_buffer[pos++];
                }
            }

            // Payload data
            if (encoded_buffer.size() < pos + payload_length) return false;
            data_frame.payload.reserve(payload_length);
            for (size_t i = 0; i < payload_length; ++i) {
                data_frame.payload.push_back(encoded_buffer[pos++]);
                if (data_frame.data_frame.mask) {
                    data_frame.payload[i] ^= data_frame.data_frame.masking_key[i % 4];
                }
            }

            return true;
        }

        std::array<uint8_t, 20> sha1(const std::string &input) {
            const uint32_t h0 = 0x67452301;
            const uint32_t h1 = 0xEFCDAB89;
            const uint32_t h2 = 0x98BADCFE;
            const uint32_t h3 = 0x10325476;
            const uint32_t h4 = 0xC3D2E1F0;

            uint64_t bit_length = input.size() * 8;
            std::string padded_input = input;

            padded_input += static_cast<unsigned char>(0x80);

            while ((padded_input.size() % 64) != 56) {
                padded_input += static_cast<char>(0x00);
            }

            for (int i = 7; i >= 0; --i) {
                padded_input += static_cast<char>((bit_length >> (i * 8)) & 0xFF);
            }

            uint32_t h[5] = {h0, h1, h2, h3, h4};

            for (size_t i = 0; i < padded_input.size(); i += 64) {
                uint32_t w[80] = {0};

                for (int j = 0; j < 16; ++j) {
                    w[j] = static_cast<uint8_t>(padded_input[i + j * 4]) << 24 |
                           static_cast<uint8_t>(padded_input[i + j * 4 + 1]) << 16 |
                           static_cast<uint8_t>(padded_input[i + j * 4 + 2]) << 8 |
                           static_cast<uint8_t>(padded_input[i + j * 4 + 3]);
                }

                for (int j = 16; j < 80; ++j) {
                    w[j] = (w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16]);
                    w[j] = (w[j] << 1) | (w[j] >> 31);
                }

                uint32_t a = h[0];
                uint32_t b = h[1];
                uint32_t c = h[2];
                uint32_t d = h[3];
                uint32_t e = h[4];

                for (int j = 0; j < 80; ++j) {
                    uint32_t f, k;

                    if (j < 20) {
                        f = (b & c) | ((~b) & d);
                        k = 0x5A827999;
                    } else if (j < 40) {
                        f = b ^ c ^ d;
                        k = 0x6ED9EBA1;
                    } else if (j < 60) {
                        f = (b & c) | (b & d) | (c & d);
                        k = 0x8F1BBCDC;
                    } else {
                        f = b ^ c ^ d;
                        k = 0xCA62C1D6;
                    }

                    uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[j];
                    e = d;
                    d = c;
                    c = (b << 30) | (b >> 2);
                    b = a;
                    a = temp;
                }

                h[0] += a;
                h[1] += b;
                h[2] += c;
                h[3] += d;
                h[4] += e;
            }

            std::array<uint8_t, 20> hash;
            for (int i = 0; i < 5; ++i) {
                hash[i * 4] = (h[i] >> 24) & 0xFF;
                hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
                hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
                hash[i * 4 + 3] = h[i] & 0xFF;
            }

            return hash;
        }

        std::string base64_encode(const uint8_t *input, size_t length) {
            static const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

            std::string result;
            int i = 0;
            uint32_t octet_a, octet_b, octet_c, triple;

            while (length >= 3) {
                octet_a = input[i++];
                octet_b = input[i++];
                octet_c = input[i++];

                triple = (octet_a << 16) + (octet_b << 8) + octet_c;

                result.push_back(base64_chars[(triple >> 18) & 0x3F]);
                result.push_back(base64_chars[(triple >> 12) & 0x3F]);
                result.push_back(base64_chars[(triple >> 6) & 0x3F]);
                result.push_back(base64_chars[triple & 0x3F]);

                length -= 3;
            }

            if (length > 0) {
                octet_a = input[i++];
                octet_b = length > 1 ? input[i++] : 0;

                triple = (octet_a << 16) + (octet_b << 8);

                result.push_back(base64_chars[(triple >> 18) & 0x3F]);
                result.push_back(base64_chars[(triple >> 12) & 0x3F]);
                result.push_back(length == 2 ? base64_chars[(triple >> 6) & 0x3F] : '=');
                result.push_back('=');
            }

            return result;
        }

        std::string generate_accept_key(const std::string &sec_websocket_key) {
            const std::string magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
            std::string concatenated = sec_websocket_key + magic_string;
            auto hash = sha1(concatenated);
            return base64_encode(hash.data(), hash.size());
        }

        void package_handshake(const Server::FRequest &req, Server::FResponse &res, const socket_ptr &socket,
                               const uint32_t status_code = 101) {
            if (req.headers.contains("Sec-WebSocket-Key")) {
                std::string key = req.headers.at("Sec-WebSocket-Key")[0];
                std::string accept_key = generate_accept_key(key);
                res.headers.insert_or_assign("Sec-WebSocket-Accept", accept_key);
            }
            if (req.headers.contains("Sec-WebSocket-Protocol")) {
                std::string protocol = req.headers.at("Sec-WebSocket-Protocol")[0];
                if (protocol.find("chat") != protocol.npos || protocol.find("superchat") != protocol.npos) {
                    res.headers.insert_or_assign("Sec-WebSocket-Protocol", "chat");
                } else if (protocol.find("json") != protocol.npos) {
                    res.headers.insert_or_assign("Sec-WebSocket-Protocol", "json");
                } else if (protocol.find("xml") != protocol.npos) {
                    res.headers.insert_or_assign("Sec-WebSocket-Protocol", "xml");
                }
            }
            std::string payload = "HTTP/" + res.version + " 101 Switching Protocols\r\n";
            if (!res.headers.empty()) {
                for (const std::pair<std::string, std::string> header: res.headers) {
                    payload += header.first + ": " + header.second + "\r\n";
                }
            }
            payload += "\r\n";
            asio::async_write(
                *socket, asio::buffer(payload.data(), payload.size()),
                std::bind(&WebsocketServer::write_handshake, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, socket, status_code));
        }

        void package_handshake_error(const uint32_t status_code, const std::string &body, const socket_ptr &socket) {
            std::string payload;
            if (ResponseStatusCode.contains(status_code)) {
                payload = "HTTP/1.1 " + std::to_string(status_code) + " " + ResponseStatusCode.at(status_code) + "\r\n";
            } else {
                payload = "HTTP/1.1 400 HTTP Bad Request\r\n";
            }
            std::string body_content = body.empty() ? body : "";
            std::string len = std::to_string(body_content.length());
            switch (status_code) {
                case 400:
                    payload += "Content-Type: text/plain\r\n"
                            "Content-Length: " + len + "\r\n"
                            "Connection: close\r\n"
                            "\r\n";
                    payload += body_content;
                    payload += "\r\n";
                    break;
                case 405:
                    payload += "Allow: GET\r\n"
                            "Content-Length: 0\r\n"
                            "Connection: close\r\n\r\n";
                    break;
                case 505:
                    payload += "Content-Type: text/plain\r\n"
                            "Content-Length: 27\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "HTTP version not supported.\r\n";
                    break;
                default: payload += "\r\n";
            }
            asio::async_write(
                *socket, asio::buffer(payload.data(), payload.size()),
                std::bind(&WebsocketServer::write_handshake, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, socket, status_code));
        }

        void consume_listening_buffers(const socket_ptr &socket) {
            const size_t res_size = listening_buffers.at(socket)->size();
            if (res_size > 0)
                listening_buffers.at(socket)->consume(res_size);
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            error_code.clear();
            socket_ptr conn_socket = std::make_shared<asio::ip::tcp::socket>(tcp.context);
            tcp.acceptor.async_accept(
                *conn_socket, std::bind(&WebsocketServer::accept, this, asio::placeholders::error, conn_socket));
            tcp.context.run();
            if (get_acceptor().is_open() && !is_closing)
                close();
        }

        void accept(const asio::error_code &error, socket_ptr &socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, socket);
                if (tcp.acceptor.is_open()) {
                    socket_ptr conn_socket = std::make_shared<asio::ip::tcp::socket>(tcp.context);
                    tcp.acceptor.async_accept(
                        *conn_socket, std::bind(&WebsocketServer::accept, this, asio::placeholders::error, conn_socket));
                }
                return;
            }

            if (tcp.sockets.size() <= backlog) {
                tcp.sockets.insert(socket);
                std::shared_ptr<asio::streambuf> listening_buffer = std::make_shared<asio::streambuf>();
                listening_buffers.insert_or_assign(socket, listening_buffer);
                asio::async_read_until(*socket, *listening_buffer, "\r\n",
                                       std::bind(&WebsocketServer::read_handshake, this,
                                                 asio::placeholders::error,
                                                 asio::placeholders::bytes_transferred, socket));
            }
            else {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (!is_closing)
                    disconnect_socket_after_error(error, socket);
            }

            if (tcp.acceptor.is_open()) {
                socket_ptr conn_socket = std::make_shared<asio::ip::tcp::socket>(tcp.context);
                tcp.acceptor.async_accept(
                    *conn_socket, std::bind(&WebsocketServer::accept, this, asio::placeholders::error, conn_socket));
            }
        }

        void read_handshake(const asio::error_code &error, const size_t bytes_recvd,
                            socket_ptr &socket) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, socket);
                return;
            }
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            std::istream response_stream(listening_buffers.at(socket).get());
            std::string method;
            response_stream >> method;
            std::string path;
            response_stream >> path;
            std::string version;
            response_stream >> version;

            std::string error_payload;
            if (method != "GET") {
                package_handshake_error(405, "", socket);
                return;
            }
            if (version != "HTTP/1.1" && version != "HTTP/2.0") {
                package_handshake_error(405, "", socket);
                return;
            }

            Server::FRequest request;
            version.erase(0, 5);
            request.version = version;
            request.method = EMethod::GET;
            request.path = path;
            if (listening_buffers.at(socket)->size() <= 2) {
                package_handshake_error(400, "Invalid handshake.", socket);
                return;
            }
            listening_buffers.at(socket)->consume(2);
            asio::async_read_until(
                *socket, *listening_buffers.at(socket), "\r\n\r\n",
                std::bind(&WebsocketServer::read_headers, this, asio::placeholders::error, request, socket));
        }

        void read_headers(const asio::error_code &error, Server::FRequest request, socket_ptr &socket) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, socket);
                return;
            }
            std::istream response_stream(&*listening_buffers.at(socket));
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                Server::req_append_header(request, header);

            consume_listening_buffers(socket);
            Server::FResponse response = res_handshake;
            response.version = request.version;
            if (!request.headers.contains("Connection"))
            {
                package_handshake_error(400, "Invalid handshake: \"Connection\" header is not set.", socket);
                return;
            }
            if (request.headers.at("Connection")[0] != "Upgrade")
            {
                package_handshake_error(400, "Invalid handshake: \"Connection\" header must be \"Upgrade\".", socket);
                return;
            }
            if (!request.headers.contains("Upgrade"))
            {
                package_handshake_error(400, "Invalid handshake: \"Upgrade\" header is not set.", socket);
                return;
            }
            if (request.headers.at("Upgrade")[0] != "websocket")
            {
                package_handshake_error(400, "Invalid handshake: \"Upgrade\" header must be \"websocket\".", socket);
                return;
            }
            if (!request.headers.contains("Sec-WebSocket-Version"))
            {
                package_handshake_error(400, "Invalid handshake: \"Sec-WebSocket-Version\" header is not set.", socket);
                return;
            }
            if (request.headers.at("Sec-WebSocket-Version")[0] != "13")
            {
                package_handshake_error(400, "Invalid handshake: \"Sec-WebSocket-Version\" header must be \"13\".", socket);
                return;
            }

            if (on_socket_accepted) {
                on_socket_accepted(request, res_handshake, socket);
            } else {
                package_handshake(request, response, socket);
            }
        }

        void write_handshake(const asio::error_code &error, const size_t bytes_sent, const socket_ptr &socket,
                             const uint32_t status_code) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, socket);
                return;
            }
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (status_code != 101) {
                if (!is_closing)
                    disconnect_socket(socket);
                return;
            }
            asio::async_read(
                *socket, *listening_buffers.at(socket), asio::transfer_at_least(1),
                std::bind(&WebsocketServer::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, socket));
        }

        void write(const asio::error_code &error, const std::size_t bytes_sent, const socket_ptr &socket) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_message_sent) on_message_sent(error, socket);
                return;
            }
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (on_message_sent) on_message_sent(error, socket);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd, const socket_ptr &socket) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, socket);
                return;
            }
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            FWsMessage rDataFrame;
            if (!decode_payload(rDataFrame, socket)) {
                consume_listening_buffers(socket);
                asio::async_read(
                    *socket, *listening_buffers.at(socket), asio::transfer_at_least(1),
                    std::bind(&WebsocketServer::read, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, socket));
                return;
            }
            if (rDataFrame.data_frame.opcode == EOpcode::PING) {
                std::vector<uint8_t> pong_buffer = { 'p', 'o', 'n', 'g', '\0' };
                post_buffer(EOpcode::PONG, pong_buffer, socket);
            } else if (rDataFrame.data_frame.opcode == EOpcode::PONG) {
                if (on_pong_received) on_pong_received(socket);
            } else if (rDataFrame.data_frame.opcode == EOpcode::CONNECTION_CLOSE) {
                if (on_close_notify) on_close_notify(socket);
            } else {
                rDataFrame.size = bytes_recvd;
                if (on_message_received) on_message_received(rDataFrame, socket);
            }

            consume_listening_buffers(socket);
            asio::async_read(
                *socket, *listening_buffers.at(socket), asio::transfer_at_least(1),
                std::bind(&WebsocketServer::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, socket));
        }
    };
#ifdef ASIO_USE_OPENSSL
    class WebsocketServerSsl {
    public:
        WebsocketServerSsl() {
            res_handshake.headers["Connection"] = "Upgrade";
            res_handshake.headers["Sec-WebSocket-Accept"] = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
            res_handshake.headers["Upgrade"] = "websocket";
            sdata_frame.mask = false;
        }

        ~WebsocketServerSsl() {
            if (get_acceptor().is_open()) close();
        }

        /*HOST*/
        void set_socket(const EProtocolType protocol, const uint16_t port,
                        const int max_listen_conn = 2147483647) {
            tcp_protocol = protocol;
            tcp_port = port;
            backlog = max_listen_conn;
        }


        asio::ssl::context &get_ssl_context() { return tcp.ssl_context; }
        asio::ip::tcp::acceptor &get_acceptor() { return tcp.acceptor; }

        const std::set<ssl_socket_ptr> get_ssl_sockets() const {
            return tcp.ssl_sockets;
        }

        /*SETTINGS*/
        void set_max_send_buffer_size(int value = 1400) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*HANDSHAKE*/
        void append_headers(const std::string &key, const std::string &value) {
            res_handshake.headers[key] = value;
        }

        void clear_headers() { res_handshake.headers.clear(); }

        void remove_param(const std::string &key) {
            if (!res_handshake.headers.contains(key)) return;
            res_handshake.headers.erase(key);
        }

        bool has_param(const std::string &key) const {
            return res_handshake.headers.contains(key);
        }

        const std::string &get_header(const std::string &key) {
            return res_handshake.headers[key];
        }

        /*DATAFRAME*/
        void set_RSV1(bool value = false) { sdata_frame.rsv1 = value; }
        bool use_RSV1() const { return sdata_frame.rsv1; }
        void set_RSV2(bool value = false) { sdata_frame.rsv2 = value; }
        bool use_RSV2() const { return sdata_frame.rsv2; }
        void set_RSV3(bool value = false) { sdata_frame.rsv3 = value; }
        bool use_RSV3() const { return sdata_frame.rsv3; }

        /*SECURITY LAYER*/
        bool load_private_key_data(const std::string &key_data) {
            if (key_data.empty()) return false;
            const asio::const_buffer buffer(key_data.data(), key_data.size());
            tcp.ssl_context.use_private_key(buffer, asio::ssl::context::pem, error_code);
            if (error_code) {
                if (on_error) on_error(error_code);
                return false;
            }
            return true;
        }

        bool load_private_key_file(const std::string &filename) {
            if (filename.empty()) return false;
            tcp.ssl_context.use_private_key_file(filename, asio::ssl::context::pem, error_code);
            if (error_code) {
                if (on_error) on_error(error_code);
                return false;
            }
            return true;
        }

        bool load_certificate_data(const std::string &cert_data) {
            if (cert_data.empty()) return false;
            const asio::const_buffer buffer(cert_data.data(), cert_data.size());
            tcp.ssl_context.use_certificate(buffer, asio::ssl::context::pem, error_code);
            if (error_code) {
                if (on_error) on_error(error_code);
                return false;
            }
            return true;
        }

        bool load_certificate_file(const std::string &filename) {
            if (filename.empty()) return false;
            tcp.ssl_context.use_certificate_file(filename, asio::ssl::context::pem, error_code);
            if (error_code) {
                if (on_error) on_error(error_code);
                return false;
            }
            return true;
        }

        bool load_certificate_chain_data(const std::string &cert_chain_data) {
            if (cert_chain_data.empty()) return false;
            const asio::const_buffer buffer(cert_chain_data.data(), cert_chain_data.size());
            tcp.ssl_context.use_certificate_chain(buffer, error_code);
            if (error_code) {
                if (on_error) on_error(error_code);
                return false;
            }
            return true;
        }

        bool load_certificate_chain_file(const std::string &filename) {
            if (filename.empty()) return false;
            tcp.ssl_context.use_certificate_chain_file(filename, error_code);
            if (error_code) {
                if (on_error) on_error(error_code);
                return false;
            }
            return true;
        }

        bool load_verify_file(const std::string &filename) {
            if (filename.empty()) return false;
            tcp.ssl_context.load_verify_file(filename, error_code);
            if (error_code) {
                if (on_error) on_error(error_code);
                return false;
            }
            return true;
        }

        /*MESSAGE*/
        bool send_handshake(const Server::FRequest &request, Server::FResponse &response,
                            const ssl_socket_ptr &ssl_socket) {
            if (!ssl_socket->next_layer().is_open()) return false;

            asio::post(thread_pool,
                       std::bind(&WebsocketServerSsl::package_handshake, this, request, response, ssl_socket, 101));
            return true;
        }

        bool send_handshake_error(const uint32_t status_code, const std::string &body, const ssl_socket_ptr &ssl_socket) {
            if (!ssl_socket->next_layer().is_open()) return false;

            asio::post(thread_pool,
                       std::bind(&WebsocketServerSsl::package_handshake_error, this, status_code, body, ssl_socket));
            return true;
        }

        bool send_str_to(const std::string &message, const ssl_socket_ptr &ssl_socket) {
            if (!ssl_socket->next_layer().is_open() || message.empty()) return false;

            asio::post(thread_pool, std::bind(&WebsocketServerSsl::post_string, this, message, ssl_socket));
            return true;
        }

        bool send_buffer_to(const std::vector<uint8_t> &buffer, const ssl_socket_ptr &ssl_socket) {
            if (!ssl_socket->next_layer().is_open() || buffer.empty()) return false;

            asio::post(thread_pool, std::bind(&WebsocketServerSsl::post_buffer, this,
                                              EOpcode::BINARY_FRAME, buffer, ssl_socket));
            return true;
        }

        bool send_ping_to(const ssl_socket_ptr &ssl_socket) {
            if (!ssl_socket->next_layer().is_open()) return false;

            std::vector<uint8_t> ping_buffer = {
                uint8_t('p'), uint8_t('i'), uint8_t('n'), uint8_t('g'), uint8_t('\0')
            };
            asio::post(thread_pool, std::bind(&WebsocketServerSsl::post_buffer, this,
                                              EOpcode::PING, ping_buffer, ssl_socket));
            return true;
        }

        /*CONNECTION*/
        bool open() {
            if (get_acceptor().is_open())
                return false;

            asio::ip::tcp::endpoint endpoint(tcp_protocol == EProtocolType::V4
                                                 ? asio::ip::tcp::v4()
                                                 : asio::ip::tcp::v6(), tcp_port);
            error_code = asio::error_code();
            tcp.acceptor.open(tcp_protocol == EProtocolType::V4
                                  ? asio::ip::tcp::v4()
                                  : asio::ip::tcp::v6(), error_code);
            if (error_code && on_error) {
                on_error(error_code);
                return false;
            }
            tcp.acceptor.set_option(asio::socket_base::reuse_address(true), error_code);
            if (error_code && on_error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }
            tcp.acceptor.bind(endpoint, error_code);
            if (error_code && on_error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }
            tcp.acceptor.listen(backlog, error_code);
            if (error_code && on_error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }

            asio::post(thread_pool, std::bind(&WebsocketServerSsl::run_context_thread, this));
            return true;
        }

        void close() {
            is_closing = true;
            if (!tcp.ssl_sockets.empty()) {
                for (const ssl_socket_ptr &ssl_socket: tcp.ssl_sockets) {
                    std::lock_guard<std::mutex> guard(mutex_error);
                    bool has_error = false;
                    if (ssl_socket->next_layer().is_open()) {
                        ssl_socket->shutdown(error_code);
                        if (on_socket_disconnected && error_code) {
                            has_error = true;
                            on_socket_disconnected(error_code, ssl_socket);
                        }
                        ssl_socket->next_layer().close(error_code);
                        if (on_socket_disconnected && error_code) {
                            has_error = true;
                            on_socket_disconnected(error_code, ssl_socket);
                        }
                        if (!has_error) {
                            on_socket_disconnected(asio::error_code(), ssl_socket);
                        }
                    }
                }
            }
            tcp.context.stop();
            if (!tcp.ssl_sockets.empty()) tcp.ssl_sockets.clear();
            if (!listening_buffers.empty()) listening_buffers.clear();
            if (tcp.acceptor.is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                tcp.acceptor.close(error_code);
                if (error_code) {
                    if (on_error) on_error(error_code);
                }
            }
            tcp.context.restart();
            tcp.acceptor = asio::ip::tcp::acceptor(tcp.context);
            if (on_close) on_close();
            is_closing = false;
        }

        void disconnect_socket(const ssl_socket_ptr &ssl_socket) {
            bool has_error = false;
            if (ssl_socket->next_layer().is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                ssl_socket->shutdown(error_code);
                if (on_socket_disconnected && error_code) {
                    has_error = true;
                    on_socket_disconnected(error_code, ssl_socket);
                }
                ssl_socket->next_layer().close(error_code);
                if (on_socket_disconnected && error_code) {
                    has_error = true;
                    on_socket_disconnected(error_code, ssl_socket);
                }
            }
            if (listening_buffers.contains(ssl_socket)) listening_buffers.erase(ssl_socket);
            if (tcp.ssl_sockets.contains(ssl_socket)) tcp.ssl_sockets.erase(ssl_socket);
            if (on_socket_disconnected && !has_error) on_socket_disconnected(asio::error_code(), ssl_socket);
        }

        /*EVENTS*/
        std::function<void(const Server::FRequest, const Server::FResponse, const ssl_socket_ptr &)> on_socket_accepted;
        std::function<void(const size_t, const size_t)> on_bytes_transfered;
        std::function<void(const asio::error_code &, const ssl_socket_ptr &)> on_message_sent;
        std::function<void(const FWsMessage, const ssl_socket_ptr &)> on_message_received;
        std::function<void(const ssl_socket_ptr &ssl_socket)> on_pong_received;
        std::function<void(const ssl_socket_ptr &ssl_socket)> on_close_notify;
        std::function<void(const asio::error_code &, const ssl_socket_ptr &)> on_socket_disconnected;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        std::mutex mutex_error;
        EProtocolType tcp_protocol = EProtocolType::V4;
        uint16_t tcp_port = 3000;
        int backlog = 2147483647;
        bool split_buffer = false;
        int max_send_buffer_size = 1400;
        bool is_closing = false;
        Server::FAsioTcpSsl tcp;
        asio::error_code error_code;
        Server::FRequest req_handshake;
        Server::FResponse res_handshake;
        std::map<ssl_socket_ptr, std::shared_ptr<asio::streambuf> > listening_buffers;
        FDataFrame sdata_frame;

        void disconnect_socket_after_error(const asio::error_code &error, const ssl_socket_ptr &ssl_socket) {
            if (ssl_socket->next_layer().is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                ssl_socket->shutdown(error_code);
                if (on_socket_disconnected && error_code) {
                    on_socket_disconnected(error_code, ssl_socket);
                }
                ssl_socket->next_layer().close(error_code);
                if (on_socket_disconnected && error_code) {
                    on_socket_disconnected(error_code, ssl_socket);
                }
            }
            if (listening_buffers.contains(ssl_socket)) listening_buffers.erase(ssl_socket);
            if (tcp.ssl_sockets.contains(ssl_socket)) tcp.ssl_sockets.erase(ssl_socket);
            if (on_socket_disconnected) on_socket_disconnected(error, ssl_socket);
        }

        void post_string(const std::string &str, const ssl_socket_ptr &ssl_socket) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            sdata_frame.opcode = EOpcode::TEXT_FRAME;
            package_string(str, ssl_socket);
        }

        void post_buffer(EOpcode opcode, const std::vector<uint8_t> &buffer, const ssl_socket_ptr &ssl_socket) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            sdata_frame.opcode = opcode;
            if (opcode == EOpcode::BINARY_FRAME) {
                package_buffer(buffer, ssl_socket);
            } else if (opcode == EOpcode::PING || opcode == EOpcode::PONG) {
                std::vector<uint8_t> p_buffer = buffer;
                encode_buffer_payload(p_buffer);
                asio::async_write(
                    *ssl_socket, asio::buffer(p_buffer.data(), p_buffer.size()),
                    std::bind(&WebsocketServerSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, ssl_socket));
            }
        }

        void package_string(const std::string &str, const ssl_socket_ptr &ssl_socket) {
            std::string payload;
            if (!split_buffer ||
                str.size() + get_frame_encode_size(str.size()) <= max_send_buffer_size) {
                sdata_frame.fin = true;
                payload = encode_string_payload(str);
                asio::async_write(
                    *ssl_socket, asio::buffer(payload.data(), payload.size()),
                    std::bind(&WebsocketServerSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, ssl_socket));
                return;
            }

            sdata_frame.fin = true;
            size_t string_offset = 0;
            const size_t max_size =
                    max_send_buffer_size - get_frame_encode_size(str.size());
            while (string_offset < str.size()) {
                sdata_frame.fin = string_offset < str.size();
                size_t package_size = std::min(max_size, str.size() - string_offset);
                payload.assign(str.begin() + string_offset,
                               str.begin() + string_offset + package_size);
                payload = encode_string_payload(payload);
                asio::async_write(
                    *ssl_socket, asio::buffer(payload, payload.size()),
                    std::bind(&WebsocketServerSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, ssl_socket));
                string_offset += package_size;
                if (sdata_frame.opcode != EOpcode::FRAME_CON)
                    sdata_frame.opcode = EOpcode::FRAME_CON;
            }
        }

        std::string encode_string_payload(const std::string &payload) {
            std::string string_buffer;
            uint64_t payload_length = payload.size();

            // FIN, RSV, Opcode
            uint8_t byte1 = sdata_frame.fin ? 0x80 : 0x00;
            byte1 |= sdata_frame.rsv1 ? (uint8_t) ERSV::RSV1 : 0x00;
            byte1 |= sdata_frame.rsv2 ? (uint8_t) ERSV::RSV2 : 0x00;
            byte1 |= sdata_frame.rsv3 ? (uint8_t) ERSV::RSV3 : 0x00;
            byte1 |= (uint8_t) sdata_frame.opcode & 0x0F;
            string_buffer.push_back(byte1);

            // Mask and payload size
            uint8_t byte2 = sdata_frame.mask ? 0x80 : 0x00;
            if (payload_length <= 125) {
                byte2 |= payload_length;
                string_buffer.push_back(byte2);
            } else if (payload_length <= 65535) {
                byte2 |= 126;
                string_buffer.push_back(byte2);
                string_buffer.push_back((payload_length >> 8) & 0xFF);
                string_buffer.push_back(payload_length & 0xFF);
            } else {
                byte2 |= 127;
                string_buffer.push_back(byte2);
                for (int i = 7; i >= 0; --i) {
                    string_buffer.push_back((payload_length >> (8 * i)) & 0xFF);
                }
            }

            std::array<uint8_t, 4> masking_key;
            if (sdata_frame.mask) {
                masking_key = mask_gen();
                for (uint8_t key: masking_key)
                    string_buffer.push_back(static_cast<uint8_t>(key));
            }

            // payload data and mask
            for (size_t i = 0; i < payload.size(); ++i) {
                if (sdata_frame.mask) {
                    string_buffer.push_back(payload[i] ^
                                            static_cast<uint8_t>(masking_key[i % 4]));
                } else {
                    string_buffer.push_back(payload[i]);
                }
            }

            return string_buffer;
        }

        void package_buffer(const std::vector<uint8_t> &buffer, const ssl_socket_ptr &ssl_socket) {
            std::vector<uint8_t> payload;
            if (!split_buffer || buffer.size() + get_frame_encode_size(buffer.size()) <=
                max_send_buffer_size) {
                sdata_frame.fin = true;
                payload = encode_buffer_payload(buffer);
                asio::async_write(
                    *ssl_socket, asio::buffer(payload.data(), payload.size()),
                    std::bind(&WebsocketServerSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, ssl_socket));
                return;
            }

            sdata_frame.fin = false;
            size_t buffer_offset = 0;
            const size_t max_size =
                    max_send_buffer_size - get_frame_encode_size(buffer.size());
            while (buffer_offset < buffer.size()) {
                sdata_frame.fin = buffer_offset < buffer.size();
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                payload.assign(buffer.begin() + buffer_offset,
                               buffer.begin() + buffer_offset + package_size);
                encode_buffer_payload(payload);
                asio::async_write(
                    *ssl_socket, asio::buffer(payload, payload.size()),
                    std::bind(&WebsocketServerSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, ssl_socket));
                buffer_offset += package_size;
                if (sdata_frame.opcode != EOpcode::FRAME_CON)
                    sdata_frame.opcode = EOpcode::FRAME_CON;
            }
        }

        std::vector<uint8_t> encode_buffer_payload(
            const std::vector<uint8_t> &payload) {
            std::vector<uint8_t> buffer;
            uint64_t payload_length = payload.size();

            // FIN, RSV, Opcode
            uint8_t byte1 = uint8_t(sdata_frame.fin ? 0x80 : 0x00);
            byte1 |= uint8_t(sdata_frame.rsv1 ? (uint8_t) ERSV::RSV1 : 0x00);
            byte1 |= uint8_t(sdata_frame.rsv2 ? (uint8_t) ERSV::RSV2 : 0x00);
            byte1 |= uint8_t(sdata_frame.rsv3 ? (uint8_t) ERSV::RSV3 : 0x00);
            byte1 |= uint8_t((uint8_t) sdata_frame.opcode & 0x0F);
            buffer.push_back(byte1);

            // Mask and payload size
            uint8_t byte2 = uint8_t(sdata_frame.mask ? 0x80 : 0x00);
            if (payload_length <= 125) {
                byte2 |= uint8_t(payload_length);
                buffer.push_back(byte2);
            } else if (payload_length <= 65535) {
                byte2 |= uint8_t(126);
                buffer.push_back(byte2);
                buffer.push_back(uint8_t((payload_length >> 8) & 0xFF));
                buffer.push_back(uint8_t(payload_length & 0xFF));
            } else {
                byte2 |= uint8_t(127);
                buffer.push_back(byte2);
                for (int i = 7; i >= 0; --i) {
                    buffer.push_back(uint8_t((payload_length >> (8 * i)) & 0xFF));
                }
            }

            std::array<uint8_t, 4> masking_key;
            if (sdata_frame.mask) {
                masking_key = mask_gen();
                for (uint8_t key: masking_key) buffer.push_back(key);
            }

            // payload data and mask
            for (size_t i = 0; i < payload.size(); ++i) {
                if (sdata_frame.mask) {
                    buffer.push_back(payload[i] ^ masking_key[i % 4]);
                } else {
                    buffer.push_back(payload[i]);
                }
            }

            return buffer;
        }

        size_t get_frame_encode_size(const size_t buffer_size) {
            size_t size = 2;
            if (buffer_size <= 125) {
                size += 0;
            } else if (buffer_size <= 65535) {
                size += 2;
            } else {
                size += 8;
            }

            if (sdata_frame.mask) size += 4;

            return size;
        }

        std::array<uint8_t, 4> mask_gen() {
            std::array<uint8_t, 4> maskKey;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);

            for (uint8_t &byte: maskKey) {
                byte = uint8_t(dis(gen));
            }

            return maskKey;
        }

        bool decode_payload(FWsMessage &data_frame, const ssl_socket_ptr &ssl_socket) {
            if (asio::buffer_size(listening_buffers.at(ssl_socket)->data()) < 2) return false;

            size_t size = asio::buffer_size(listening_buffers.at(ssl_socket)->data());
            std::vector<uint8_t> encoded_buffer;
            encoded_buffer.resize(size);
            asio::buffer_copy(asio::buffer(encoded_buffer, encoded_buffer.size()),
                              listening_buffers.at(ssl_socket)->data());

            size_t pos = 0;
            // FIN, RSV, Opcode
            uint8_t byte1 = encoded_buffer[pos++];
            data_frame.data_frame.fin = (uint8_t) byte1 & 0x80;
            data_frame.data_frame.rsv1 = (uint8_t) byte1 & 0x80;
            data_frame.data_frame.rsv2 = (uint8_t) byte1 & 0x40;
            data_frame.data_frame.rsv3 = (uint8_t) byte1 & 0x10;
            data_frame.data_frame.opcode = (EOpcode) ((uint8_t) byte1 & 0x0F);

            // Mask and payload length
            uint8_t byte2 = encoded_buffer[pos++];
            data_frame.data_frame.mask = (uint8_t) byte2 & 0x80;
            uint64_t payload_length = (uint8_t) byte2 & 0x7F;
            if (payload_length == 126) {
                if (encoded_buffer.size() < pos + 2) return false;
                payload_length = static_cast<uint64_t>((encoded_buffer[pos] << 8) |
                                                       encoded_buffer[pos + 1]);
                pos += 2;
            } else if (payload_length == 127) {
                if (encoded_buffer.size() < pos + 8) return false;
                payload_length = 0;
                for (int i = 0; i < 8; ++i) {
                    payload_length = static_cast<uint64_t>(
                        (uint8_t(payload_length) << 8) | encoded_buffer[pos + i]);
                }
                pos += 8;
            }
            data_frame.data_frame.length = payload_length;

            // Masking key
            if (data_frame.data_frame.mask) {
                if (encoded_buffer.size() < pos + 4) return false;
                for (int i = 0; i < 4; ++i) {
                    data_frame.data_frame.masking_key[i] = encoded_buffer[pos++];
                }
            }

            // Payload data
            if (encoded_buffer.size() < pos + payload_length) return false;
            data_frame.payload.reserve(payload_length);
            for (size_t i = 0; i < payload_length; ++i) {
                data_frame.payload.push_back(encoded_buffer[pos++]);
                if (data_frame.data_frame.mask) {
                    data_frame.payload[i] ^= data_frame.data_frame.masking_key[i % 4];
                }
            }

            return true;
        }

        std::array<uint8_t, 20> sha1(const std::string &input) {
            const uint32_t h0 = 0x67452301;
            const uint32_t h1 = 0xEFCDAB89;
            const uint32_t h2 = 0x98BADCFE;
            const uint32_t h3 = 0x10325476;
            const uint32_t h4 = 0xC3D2E1F0;

            uint64_t bit_length = input.size() * 8;
            std::string padded_input = input;

            padded_input += static_cast<unsigned char>(0x80);

            while ((padded_input.size() % 64) != 56) {
                padded_input += static_cast<char>(0x00);
            }

            for (int i = 7; i >= 0; --i) {
                padded_input += static_cast<char>((bit_length >> (i * 8)) & 0xFF);
            }

            uint32_t h[5] = {h0, h1, h2, h3, h4};

            for (size_t i = 0; i < padded_input.size(); i += 64) {
                uint32_t w[80] = {0};

                for (int j = 0; j < 16; ++j) {
                    w[j] = static_cast<uint8_t>(padded_input[i + j * 4]) << 24 |
                           static_cast<uint8_t>(padded_input[i + j * 4 + 1]) << 16 |
                           static_cast<uint8_t>(padded_input[i + j * 4 + 2]) << 8 |
                           static_cast<uint8_t>(padded_input[i + j * 4 + 3]);
                }

                for (int j = 16; j < 80; ++j) {
                    w[j] = (w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16]);
                    w[j] = (w[j] << 1) | (w[j] >> 31);
                }

                uint32_t a = h[0];
                uint32_t b = h[1];
                uint32_t c = h[2];
                uint32_t d = h[3];
                uint32_t e = h[4];

                for (int j = 0; j < 80; ++j) {
                    uint32_t f, k;

                    if (j < 20) {
                        f = (b & c) | ((~b) & d);
                        k = 0x5A827999;
                    } else if (j < 40) {
                        f = b ^ c ^ d;
                        k = 0x6ED9EBA1;
                    } else if (j < 60) {
                        f = (b & c) | (b & d) | (c & d);
                        k = 0x8F1BBCDC;
                    } else {
                        f = b ^ c ^ d;
                        k = 0xCA62C1D6;
                    }

                    uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[j];
                    e = d;
                    d = c;
                    c = (b << 30) | (b >> 2);
                    b = a;
                    a = temp;
                }

                h[0] += a;
                h[1] += b;
                h[2] += c;
                h[3] += d;
                h[4] += e;
            }

            std::array<uint8_t, 20> hash;
            for (int i = 0; i < 5; ++i) {
                hash[i * 4] = (h[i] >> 24) & 0xFF;
                hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
                hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
                hash[i * 4 + 3] = h[i] & 0xFF;
            }

            return hash;
        }

        std::string base64_encode(const uint8_t *input, size_t length) {
            static const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

            std::string result;
            int i = 0;
            uint32_t octet_a, octet_b, octet_c, triple;

            while (length >= 3) {
                octet_a = input[i++];
                octet_b = input[i++];
                octet_c = input[i++];

                triple = (octet_a << 16) + (octet_b << 8) + octet_c;

                result.push_back(base64_chars[(triple >> 18) & 0x3F]);
                result.push_back(base64_chars[(triple >> 12) & 0x3F]);
                result.push_back(base64_chars[(triple >> 6) & 0x3F]);
                result.push_back(base64_chars[triple & 0x3F]);

                length -= 3;
            }

            if (length > 0) {
                octet_a = input[i++];
                octet_b = length > 1 ? input[i++] : 0;

                triple = (octet_a << 16) + (octet_b << 8);

                result.push_back(base64_chars[(triple >> 18) & 0x3F]);
                result.push_back(base64_chars[(triple >> 12) & 0x3F]);
                result.push_back(length == 2 ? base64_chars[(triple >> 6) & 0x3F] : '=');
                result.push_back('=');
            }

            return result;
        }

        std::string generate_accept_key(const std::string &sec_websocket_key) {
            const std::string magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
            std::string concatenated = sec_websocket_key + magic_string;
            auto hash = sha1(concatenated);
            return base64_encode(hash.data(), hash.size());
        }

        void package_handshake(const Server::FRequest &req, Server::FResponse &res, const ssl_socket_ptr &ssl_socket,
                               const uint32_t status_code = 101) {
            if (req.headers.contains("Sec-WebSocket-Key")) {
                std::string key = req.headers.at("Sec-WebSocket-Key")[0];
                std::string accept_key = generate_accept_key(key);
                res.headers.insert_or_assign("Sec-WebSocket-Accept", accept_key);
            }
            if (req.headers.contains("Sec-WebSocket-Protocol")) {
                std::string protocol = req.headers.at("Sec-WebSocket-Protocol")[0];
                std::cout << protocol << std::endl;
                if (protocol.find("chat") != protocol.npos || protocol.find("superchat") != protocol.npos) {
                    res.headers.insert_or_assign("Sec-WebSocket-Protocol", "chat");
                } else if (protocol.find("json") != protocol.npos) {
                    res.headers.insert_or_assign("Sec-WebSocket-Protocol", "json");
                } else if (protocol.find("xml") != protocol.npos) {
                    res.headers.insert_or_assign("Sec-WebSocket-Protocol", "xml");
                }
            }
            std::string payload = "HTTP/" + res.version + " 101 Switching Protocols\r\n";
            if (!res.headers.empty()) {
                for (const std::pair<std::string, std::string> header: res.headers) {
                    payload += header.first + ": " + header.second + "\r\n";
                }
            }
            payload += "\r\n";
            asio::async_write(
                *ssl_socket, asio::buffer(payload.data(), payload.size()),
                std::bind(&WebsocketServerSsl::write_handshake, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, ssl_socket, status_code));
        }

        void package_handshake_error(const uint32_t status_code, const std::string &body, const ssl_socket_ptr &ssl_socket) {
            std::string payload;
            if (ResponseStatusCode.contains(status_code)) {
                payload = "HTTP/1.1 " + std::to_string(status_code) + " " + ResponseStatusCode.at(status_code) + "\r\n";
            }
            else {
                payload = "HTTP/1.1 400 HTTP Bad Request\r\n";
            }
            std::string body_content = body.empty() ? body : "";
            std::string len = std::to_string(body_content.length());
            switch (status_code)
            {
                case 400:
                    payload += "Content-Type: text/plain\r\n"
                    "Content-Length: " + len + "\r\n"
                    "Connection: close\r\n"
                    "\r\n";
                payload += body_content;
                payload += "\r\n";
                break;
                case 405:
                    payload += "Allow: GET\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n\r\n";
                break;
                case 505:
                    payload += "Content-Type: text/plain\r\n"
                    "Content-Length: 27\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "HTTP version not supported.\r\n";
                break;
                default: payload += "\r\n";
            }
            asio::async_write(
                *ssl_socket, asio::buffer(payload.data(), payload.size()),
                std::bind(&WebsocketServerSsl::write_handshake, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, ssl_socket, status_code));
        }

        void consume_listening_buffers(const ssl_socket_ptr &ssl_socket) {
            const size_t res_size = listening_buffers.at(ssl_socket)->size();
            if (res_size > 0)
                listening_buffers.at(ssl_socket)->consume(res_size);
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            error_code.clear();
            ssl_socket_ptr conn_ssl_socket = std::make_shared<
                asio::ssl::stream<asio::ip::tcp::socket> >(tcp.context, tcp.ssl_context);
            tcp.acceptor.async_accept(
                conn_ssl_socket->lowest_layer(),
                std::bind(&WebsocketServerSsl::accept, this, asio::placeholders::error, conn_ssl_socket));
            tcp.context.run();
            if (get_acceptor().is_open() && !is_closing)
                close();
        }

        void accept(const asio::error_code &error, ssl_socket_ptr &ssl_socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
                if (tcp.acceptor.is_open()) {
                    ssl_socket_ptr conn_ssl_socket = std::make_shared<
                    asio::ssl::stream<asio::ip::tcp::socket> >(tcp.context, tcp.ssl_context);
                    tcp.acceptor.async_accept(
                        conn_ssl_socket->lowest_layer(),
                        std::bind(&WebsocketServerSsl::accept, this, asio::placeholders::error, conn_ssl_socket));
                }
                return;
            }

            if (tcp.ssl_sockets.size() < backlog) {
                ssl_socket->async_handshake(asio::ssl::stream_base::server,
                                        std::bind(&WebsocketServerSsl::ssl_handshake, this,
                                                  asio::placeholders::error, ssl_socket));
            }
            else {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (!is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
            }

            if (tcp.acceptor.is_open()) {
                ssl_socket_ptr conn_ssl_socket = std::make_shared<
                asio::ssl::stream<asio::ip::tcp::socket> >(tcp.context, tcp.ssl_context);
                tcp.acceptor.async_accept(
                    conn_ssl_socket->lowest_layer(),
                    std::bind(&WebsocketServerSsl::accept, this, asio::placeholders::error, conn_ssl_socket));
            }
        }

        void ssl_handshake(const asio::error_code &error, ssl_socket_ptr &ssl_socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
                return;
            }

            tcp.ssl_sockets.insert(ssl_socket);
            std::shared_ptr<asio::streambuf> listening_buffer = std::make_shared<asio::streambuf>();
            listening_buffers.insert_or_assign(ssl_socket, listening_buffer);
            asio::async_read_until(*ssl_socket, *listening_buffer, "\r\n",
                                   std::bind(&WebsocketServerSsl::read_handshake, this,
                                             asio::placeholders::error,
                                             asio::placeholders::bytes_transferred, ssl_socket));
        }

        void read_handshake(const asio::error_code &error, const size_t bytes_recvd,
                            ssl_socket_ptr &ssl_socket) {
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
                return;
            }
            std::istream response_stream(&*listening_buffers.at(ssl_socket));
            std::string method;
            response_stream >> method;
            std::string path;
            response_stream >> path;
            std::string version;
            response_stream >> version;

            std::string error_payload;
            if (method != "GET") {
                package_handshake_error(405, "", ssl_socket);
                return;
            }
            if (version != "HTTP/1.1" && version != "HTTP/2.0") {
                package_handshake_error(505, "", ssl_socket);
                return;
            }

            Server::FRequest request;
            version.erase(0, 5);
            request.version = version;
            request.method = EMethod::GET;
            request.path = path;
            if (listening_buffers.at(ssl_socket)->size() <= 2) {
                package_handshake_error(400, "Invalid handshake.", ssl_socket);
                return;
            }
            listening_buffers.at(ssl_socket)->consume(2);
            asio::async_read_until(
                *ssl_socket, *listening_buffers.at(ssl_socket), "\r\n\r\n",
                std::bind(&WebsocketServerSsl::read_headers, this, asio::placeholders::error, request, ssl_socket));
        }

        void read_headers(const asio::error_code &error, Server::FRequest request, ssl_socket_ptr &ssl_socket) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
                return;
            }
            std::istream response_stream(&*listening_buffers.at(ssl_socket));
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                Server::req_append_header(request, header);

            consume_listening_buffers(ssl_socket);
            Server::FResponse response = res_handshake;
            response.version = request.version;
            if (!request.headers.contains("Connection"))
            {
                package_handshake_error(400, "Invalid handshake: \"Connection\" header is not set.", ssl_socket);
                return;
            }
            if (request.headers.at("Connection")[0] != "Upgrade")
            {
                package_handshake_error(400, "Invalid handshake: \"Connection\" header must be \"Upgrade\".", ssl_socket);
                return;
            }
            if (!request.headers.contains("Upgrade"))
            {
                package_handshake_error(400, "Invalid handshake: \"Upgrade\" header is not set.", ssl_socket);
                return;
            }
            if (request.headers.at("Upgrade")[0] != "websocket")
            {
                package_handshake_error(400, "Invalid handshake: \"Upgrade\" header must be \"websocket\".", ssl_socket);
                return;
            }
            if (!request.headers.contains("Sec-WebSocket-Version"))
            {
                package_handshake_error(400, "Invalid handshake: \"Sec-WebSocket-Version\" header is not set.", ssl_socket);
                return;
            }
            if (request.headers.at("Sec-WebSocket-Version")[0] != "13")
            {
                package_handshake_error(400, "Invalid handshake: \"Sec-WebSocket-Version\" header must be \"13\".", ssl_socket);
                return;
            }

            if (on_socket_accepted) {
                on_socket_accepted(request, res_handshake, ssl_socket);
            } else {
                package_handshake(request, response, ssl_socket);
            }
        }

        void write_handshake(const asio::error_code &error, const size_t bytes_sent, const ssl_socket_ptr &ssl_socket,
                             const uint32_t status_code) {
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
                return;
            }
            if (status_code != 101) {
                if (ssl_socket->lowest_layer().is_open() || !is_closing)
                    disconnect_socket(ssl_socket);
                return;
            }
            asio::async_read(
                *ssl_socket, *listening_buffers.at(ssl_socket), asio::transfer_at_least(1),
                std::bind(&WebsocketServerSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, ssl_socket));
        }

        void write(const asio::error_code &error, const std::size_t bytes_sent, const ssl_socket_ptr &ssl_socket) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_message_sent) on_message_sent(error, ssl_socket);
                return;
            }
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (on_message_sent) on_message_sent(error, ssl_socket);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd, const ssl_socket_ptr &ssl_socket) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
                return;
            }
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            FWsMessage rDataFrame;
            if (!decode_payload(rDataFrame, ssl_socket)) {
                consume_listening_buffers(ssl_socket);
                asio::async_read(
                    *ssl_socket, *listening_buffers.at(ssl_socket), asio::transfer_at_least(1),
                    std::bind(&WebsocketServerSsl::read, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, ssl_socket));
                return;
            }
            if (rDataFrame.data_frame.opcode == EOpcode::PING) {
                std::vector<uint8_t> pong_buffer = {
                    uint8_t('p'), uint8_t('o'), uint8_t('n'), uint8_t('g'), uint8_t('\0')
                };
                post_buffer(EOpcode::PONG, pong_buffer, ssl_socket);
            } else if (rDataFrame.data_frame.opcode == EOpcode::PONG) {
                if (on_pong_received) on_pong_received(ssl_socket);
            } else if (rDataFrame.data_frame.opcode == EOpcode::CONNECTION_CLOSE) {
                if (on_close_notify) on_close_notify(ssl_socket);
            } else {
                rDataFrame.size = bytes_recvd;
                if (on_message_received) on_message_received(rDataFrame, ssl_socket);
            }

            consume_listening_buffers(ssl_socket);
            asio::async_read(
                *ssl_socket, *listening_buffers.at(ssl_socket), asio::transfer_at_least(1),
                std::bind(&WebsocketServerSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, ssl_socket));
        }
    };
#endif
} // namespace InternetProtocol
