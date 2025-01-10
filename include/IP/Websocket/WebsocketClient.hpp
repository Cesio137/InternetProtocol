/*
 * Copyright (c) 2023-2024 Nathan Miguel
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
    class WebsocketClient {
    public:
        WebsocketClient() {
            req_handshake.headers["Connection"] = "Upgrade";
            req_handshake.headers["Origin"] = "ASIO";
            req_handshake.headers["Sec-WebSocket-Key"] = "dGhlIHNhbXBsZSBub25jZQ==";
            req_handshake.headers["Sec-WebSocket-Protocol"] = "chat, superchat";
            req_handshake.headers["Sec-WebSocket-Version"] = "13";
            req_handshake.headers["Upgrade"] = "websocket";
        }

        ~WebsocketClient() {
            tcp.resolver.cancel();
            if (is_connected()) close();
            consume_response_buffer();
        }

        /*HOST*/
        void set_host(const std::string &url = "localhost", const std::string &port = "3000") {
            host = url;
            service = port;
        }

        asio::ip::tcp::socket &get_socket() { return tcp.socket; }

        /*SETTINGS*/
        void set_max_send_buffer_size(int value = 1400) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*HANDSHAKE*/
        void append_header(const std::string &key, const std::string &value) {
            req_handshake.headers[key] = value;
        }

        void clear_headers() { req_handshake.headers.clear(); }

        void remove_header(const std::string &key) {
            if (!req_handshake.headers.contains(key)) return;
            req_handshake.headers.erase(key);
        }

        bool has_header(const std::string &key) const {
            return req_handshake.headers.contains(key);
        }

        const std::string &get_header(const std::string &key) {
            return req_handshake.headers[key];
        }

        /*DATAFRAME*/
        void set_RSV1(bool value = false) { sdata_frame.rsv1 = value; }
        bool use_RSV1() const { return sdata_frame.rsv1; }
        void set_RSV2(bool value = false) { sdata_frame.rsv2 = value; }
        bool use_RSV2() const { return sdata_frame.rsv2; }
        void set_RSV3(bool value = false) { sdata_frame.rsv3 = value; }
        bool use_RSV3() const { return sdata_frame.rsv3; }
        void set_Mask(bool value = true) { sdata_frame.mask = value; }
        bool use_Mask() const { return sdata_frame.mask; }

        /*MESSAGE*/
        bool send_str(const std::string &message) {
            if (!is_connected() || message.empty()) return false;

            asio::post(thread_pool, std::bind(&WebsocketClient::post_string, this, message));
            return true;
        }

        bool send_buffer(const std::vector<uint8_t> &buffer) {
            if (is_connected() || buffer.empty()) return false;

            asio::post(thread_pool, std::bind(&WebsocketClient::post_buffer, this,
                                              EOpcode::BINARY_FRAME, buffer));
            return true;
        }

        bool send_ping() {
            if (!is_connected()) return false;

            std::vector<uint8_t> ping_buffer = {
                uint8_t('p'), uint8_t('i'), uint8_t('n'), uint8_t('g'), uint8_t('\0')
            };
            asio::post(thread_pool, std::bind(&WebsocketClient::post_buffer, this,
                                              EOpcode::PING, ping_buffer));
            return true;
        }

        bool async_read() {
            if (!is_connected()) return false;

            asio::async_read(
                tcp.socket, response_buffer, asio::transfer_at_least(2),
                std::bind(&WebsocketClient::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
            return true;
        }

        /*CONNECTION*/
        bool connect() {
            if (is_connected()) return false;

            asio::post(thread_pool, std::bind(&WebsocketClient::run_context_thread, this));
            return true;
        }

        bool is_connected() const { return tcp.socket.is_open(); }

        void close() {
            is_closing = true;
            tcp.context.stop();
            if (get_socket().is_open()) {
                tcp.socket.shutdown(asio::ip::udp::socket::shutdown_both, error_code);
                if (error_code && on_error) {
                    std::lock_guard<std::mutex> guard(mutex_error);
                    on_error(error_code);
                }

                tcp.socket.close(error_code);
                if (error_code && on_error) {
                    std::lock_guard<std::mutex> guard(mutex_error);
                    on_error(error_code);
                }
            }
            tcp.context.restart();
            if (on_close) on_close();
            is_closing = false;
        }

        /*ERROR*/
        asio::error_code get_error_code() const { return error_code; }


        /*EVENTS*/
        std::function<void(const Client::FResponse)> on_connected;
        std::function<void(const size_t, const size_t)> on_bytes_transfered;
        std::function<void()> on_message_sent;
        std::function<void(const FWsMessage)> on_message_received;
        std::function<void()> on_pong_received;
        std::function<void()> on_close_notify;
        std::function<void()> on_close;
        std::function<void(const int, const std::string &)> on_handshake_fail;
        std::function<void(const asio::error_code &)> on_socket_error;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        std::mutex mutex_error;
        std::string host = "localhost";
        std::string service = "3000";
        bool split_buffer = false;
        int max_send_buffer_size = 1400;
        bool is_closing = false;
        Client::FAsioTcp tcp;
        asio::error_code error_code;
        asio::streambuf response_buffer;
        Client::FRequest req_handshake;
        Client::FResponse res_handshake;
        FDataFrame sdata_frame;

        void post_string(const std::string &str) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            sdata_frame.opcode = EOpcode::TEXT_FRAME;
            package_string(str);
        }

        void post_buffer(EOpcode opcode, const std::vector<uint8_t> &buffer) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            sdata_frame.opcode = opcode;
            if (opcode == EOpcode::BINARY_FRAME) {
                package_buffer(buffer);
            } else if (opcode == EOpcode::PING || opcode == EOpcode::PONG) {
                std::vector<uint8_t> p_buffer = buffer;
                encode_buffer_payload(p_buffer);
                asio::async_write(
                    tcp.socket, asio::buffer(p_buffer.data(), p_buffer.size()),
                    std::bind(&WebsocketClient::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
            }
        }

        void package_string(const std::string &str) {
            std::string payload;
            if (!split_buffer ||
                str.size() + get_frame_encode_size(str.size()) <= max_send_buffer_size) {
                sdata_frame.fin = true;
                payload = encode_string_payload(str);
                asio::async_write(
                    tcp.socket, asio::buffer(payload.data(), payload.size()),
                    std::bind(&WebsocketClient::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
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
                    tcp.socket, asio::buffer(payload, payload.size()),
                    std::bind(&WebsocketClient::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
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

        void package_buffer(const std::vector<uint8_t> &buffer) {
            std::vector<uint8_t> payload;
            if (!split_buffer || buffer.size() + get_frame_encode_size(buffer.size()) <=
                max_send_buffer_size) {
                sdata_frame.fin = true;
                payload = encode_buffer_payload(buffer);
                asio::async_write(
                    tcp.socket, asio::buffer(payload.data(), payload.size()),
                    std::bind(&WebsocketClient::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
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
                    tcp.socket, asio::buffer(payload, payload.size()),
                    std::bind(&WebsocketClient::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
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

        bool decode_payload(FWsMessage &data_frame) {
            if (asio::buffer_size(response_buffer.data()) < 2) return false;

            size_t size = asio::buffer_size(response_buffer.data());
            std::vector<uint8_t> encoded_buffer;
            encoded_buffer.resize(size);
            asio::buffer_copy(asio::buffer(encoded_buffer, encoded_buffer.size()),
                              response_buffer.data());

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

            padded_input += static_cast<char>(0x80);

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

        void consume_response_buffer() {
            const size_t res_size = response_buffer.size();
            if (res_size > 0)
                response_buffer.consume(res_size);
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> lock(mutex_io);
            error_code.clear();
            tcp.resolver.async_resolve(
                host, service,
                std::bind(&WebsocketClient::resolve, this, asio::placeholders::error,
                          asio::placeholders::endpoint));
            tcp.context.run();
            if (get_socket().is_open() && !is_closing)
                close();
        }

        void resolve(const asio::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            tcp.endpoints = endpoints;
            asio::async_connect(
                tcp.socket, tcp.endpoints,
                std::bind(&WebsocketClient::conn, this, asio::placeholders::error));
        }

        void conn(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            std::string request;
            request = "GET " + req_handshake.path + " HTTP/" + req_handshake.version + "\r\n";
            if (!req_handshake.headers.contains("Host"))
                request += "Host: " + host + ":" + service + "\r\n";
            if (!req_handshake.headers.empty()) {
                for (const auto &header: req_handshake.headers) {
                    request += header.first + ": " + header.second + "\r\n";
                }
            }
            request += "\r\n";
            asio::async_write(tcp.socket, asio::buffer(request.data(), request.size()),
                              std::bind(&WebsocketClient::write_handshake, this,
                                        asio::placeholders::error,
                                        asio::placeholders::bytes_transferred));
        }

        void write_handshake(const asio::error_code &error, const size_t bytes_sent) {
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            asio::async_read_until(tcp.socket, response_buffer, "\r\n",
                                   std::bind(&WebsocketClient::read_handshake, this,
                                             asio::placeholders::error, asio::placeholders::bytes_transferred));
        }

        void read_handshake(const asio::error_code &error, const size_t bytes_recvd) {
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            std::istream response_stream(&response_buffer);
            std::string http_version;
            response_stream >> http_version;
            int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                if (on_handshake_fail) on_handshake_fail(505, ResponseStatusCode.at(505));
                if (get_socket().is_open() && !is_closing)
                    close();
                return;
            }
            if (status_code != 101) {
                if (on_handshake_fail) on_handshake_fail(status_code,
                                                         ResponseStatusCode.contains(status_code)
                                                             ? ResponseStatusCode.at(status_code)
                                                             : "");
                if (get_socket().is_open() && !is_closing)
                    close();
                return;
            }

            if (response_buffer.size() == 0) {
                if (on_connected) on_connected(Client::FResponse());
                asio::async_read(
                    tcp.socket, response_buffer, asio::transfer_at_least(1),
                    std::bind(&WebsocketClient::read, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                return;
            }
            asio::async_read_until(tcp.socket, response_buffer, "\r\n\r\n",
                                   std::bind(&WebsocketClient::read_headers,
                                             this, asio::placeholders::error));
        }

        void read_headers(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            Client::res_clear(res_handshake);
            std::istream response_stream(&response_buffer);
            std::string header;
            while (std::getline(response_stream, header) && header != "\r")
                Client::res_append_header(res_handshake, header);
            consume_response_buffer();

            if (res_handshake.headers.empty()) {
                if (on_handshake_fail) on_handshake_fail(-1, "Invalid header: Empty");
                if (get_socket().is_open() && !is_closing)
                    close();
                return;
            }
            if (res_handshake.headers.at("Connection")[0] != "Upgrade" || res_handshake.headers.at("Upgrade")[0] !=
                "websocket") {
                if (on_handshake_fail) on_handshake_fail(-1, "Invalid header: Connection");
                if (get_socket().is_open() && !is_closing)
                    close();
            }
            std::string protocol = req_handshake.headers.at("Sec-WebSocket-Protocol");
            std::string res_protocol = res_handshake.headers.at("Sec-WebSocket-Protocol")[0];
            if (protocol.find(res_protocol) == protocol.npos) {
                if (on_handshake_fail) on_handshake_fail(-1, "Invalid header: Sec-WebSocket-Protocol");
                if (get_socket().is_open() && !is_closing)
                    close();
            }
            std::string accept_key = res_handshake.headers.at("Sec-WebSocket-Accept")[0];
            std::string encoded_key = generate_accept_key(req_handshake.headers.at("Sec-WebSocket-Key"));
            if (accept_key != encoded_key) {
                if (on_handshake_fail) on_handshake_fail(-1, "Invalid Sec-WebSocket-Accept");
                if (get_socket().is_open() && !is_closing)
                    close();
            }

            if (on_connected) on_connected(res_handshake);
            asio::async_read(
                tcp.socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&WebsocketClient::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
        }

        void write(const asio::error_code &error, const std::size_t bytes_sent) {
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            if (on_message_sent) on_message_sent();
        }

        void read(const asio::error_code &error, const size_t bytes_recvd) {
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            FWsMessage rDataFrame;
            if (!decode_payload(rDataFrame)) {
                response_buffer.consume(response_buffer.size());
                asio::async_read(
                    tcp.socket, response_buffer, asio::transfer_at_least(1),
                    std::bind(&WebsocketClient::read, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                return;
            }
            if (rDataFrame.data_frame.opcode == EOpcode::PING) {
                std::vector<uint8_t> pong_buffer = {
                    uint8_t('p'), uint8_t('o'), uint8_t('n'), uint8_t('g'), uint8_t('\0')
                };
                post_buffer(EOpcode::PONG, pong_buffer);
            } else if (rDataFrame.data_frame.opcode == EOpcode::PONG) {
                if (on_pong_received) on_pong_received();
            } else if (rDataFrame.data_frame.opcode == EOpcode::CONNECTION_CLOSE) {
                if (on_close_notify) on_close_notify();
            } else {
                rDataFrame.size = bytes_recvd;
                if (on_message_received) on_message_received(rDataFrame);
            }

            consume_response_buffer();
            asio::async_read(
                tcp.socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&WebsocketClient::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
        }
    };
#ifdef ASIO_USE_OPENSSL
    class WebsocketClientSsl {
    public:
        WebsocketClientSsl() {
            req_handshake.path = "/chat";
            req_handshake.headers["Connection"] = "Upgrade";
            req_handshake.headers["Origin"] = "ASIO";
            req_handshake.headers["Sec-WebSocket-Key"] = "dGhlIHNhbXBsZSBub25jZQ==";
            req_handshake.headers["Sec-WebSocket-Protocol"] = "chat, superchat";
            req_handshake.headers["Sec-WebSocket-Version"] = "13";
            req_handshake.headers["Upgrade"] = "websocket";
        }

        ~WebsocketClientSsl() {
            should_stop_context = true;
            tcp.resolver.cancel();
            if (is_connected()) close();
            consume_response_buffer();
        }

        /*HOST*/
        void set_host(const std::string &url = "localhost", const std::string &port = "80") {
            host = url;
            service = port;
        }

        asio::ssl::context &get_ssl_context() { return tcp.ssl_context; }
        const asio::ssl::stream<asio::ip::tcp::socket> &get_ssl_socket() const { return tcp.ssl_socket; }

        void update_ssl_socket() {
            tcp.ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(tcp.context, tcp.ssl_context);
        }

        /*SETTINGS*/
        void set_max_send_buffer_size(int value = 1400) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*HANDSHAKE*/
        void append_header(const std::string &key, const std::string &value) {
            req_handshake.headers[key] = value;
        }

        void clear_headers() { req_handshake.headers.clear(); }

        void remove_header(const std::string &key) {
            if (!req_handshake.headers.contains(key)) return;
            req_handshake.headers.erase(key);
        }

        bool has_header(const std::string &key) const {
            return req_handshake.headers.contains(key);
        }

        const std::string &get_header(const std::string &key) {
            return req_handshake.headers[key];
        }

        /*DATAFRAME*/
        void set_RSV1(bool value = false) { sdata_frame.rsv1 = value; }
        bool use_RSV1() const { return sdata_frame.rsv1; }
        void set_RSV2(bool value = false) { sdata_frame.rsv2 = value; }
        bool use_RSV2() const { return sdata_frame.rsv2; }
        void set_RSV3(bool value = false) { sdata_frame.rsv3 = value; }
        bool use_RSV3() const { return sdata_frame.rsv3; }
        void set_Mask(bool value = true) { sdata_frame.mask = value; }
        bool use_Mask() const { return sdata_frame.mask; }

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
        bool send_str(const std::string &message) {
            if (!is_connected() || message.empty()) return false;

            asio::post(thread_pool,
                       std::bind(&WebsocketClientSsl::post_string, this, message));
            return true;
        }

        bool send_buffer(const std::vector<uint8_t> &buffer) {
            if (!is_connected() || buffer.empty()) return false;

            asio::post(thread_pool, std::bind(&WebsocketClientSsl::post_buffer, this,
                                              EOpcode::BINARY_FRAME, buffer));
            return true;
        }

        bool send_ping() {
            if (!is_connected()) return false;

            std::vector<uint8_t> ping_buffer;
            ping_buffer.push_back(uint8_t('\0'));
            asio::post(thread_pool, std::bind(&WebsocketClientSsl::post_buffer, this,
                                              EOpcode::PING, ping_buffer));
            return true;
        }

        /*CONNECTION*/
        bool connect() {
            if (is_connected()) return false;

            asio::post(thread_pool, std::bind(&WebsocketClientSsl::run_context_thread, this));
            return true;
        }

        bool is_connected() const { return tcp.ssl_socket.lowest_layer().is_open(); }

        void close() {
            is_closing = true;
            if (get_ssl_socket().next_layer().is_open()) {
                tcp.ssl_socket.shutdown(error_code);
                if (error_code && on_error) {
                    std::lock_guard<std::mutex> guard(mutex_error);
                    on_error(error_code);
                }
                tcp.ssl_socket.next_layer().close(error_code);
                if (error_code && on_error) {
                    std::lock_guard<std::mutex> guard(mutex_error);
                    on_error(error_code);
                }
            }
            tcp.context.stop();
            tcp.context.restart();
            tcp.ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(tcp.context, tcp.ssl_context);
            if (on_close) on_close();
            is_closing = false;
        }

        /*ERRORS*/
        asio::error_code get_error_code() const { return error_code; }

        /*EVENTS*/
        std::function<void(const Client::FResponse)> on_connected;
        std::function<void(const size_t, const size_t)> on_bytes_transfered;
        std::function<void(const size_t)> on_message_sent;
        std::function<void(const FWsMessage)> on_message_received;
        std::function<void()> on_pong_received;
        std::function<void()> on_close_notify;
        std::function<void()> on_close;
        std::function<void(const int, const std::string &)> on_handshake_fail;
        std::function<void(const asio::error_code &)> on_socket_error;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        std::mutex mutex_error;
        bool is_closing = false;
        bool should_stop_context = false;
        std::string host = "localhost";
        std::string service = "3000";
        bool split_buffer = false;
        int max_send_buffer_size = 1400;
        Client::FAsioTcpSsl tcp;
        asio::error_code error_code;
        asio::streambuf response_buffer;
        Client::FRequest req_handshake;
        Client::FResponse res_handshake;
        FDataFrame sdata_frame;

        void post_string(const std::string &str) {
            mutex_buffer.lock();
            sdata_frame.opcode = EOpcode::TEXT_FRAME;
            package_string(str);
            mutex_buffer.unlock();
        }

        void post_buffer(EOpcode opcode, const std::vector<uint8_t> &buffer) {
            mutex_buffer.lock();
            sdata_frame.opcode = opcode;
            if (opcode == EOpcode::BINARY_FRAME) {
                package_buffer(buffer);
            } else if (opcode == EOpcode::PING || opcode == EOpcode::PONG) {
                std::vector<uint8_t> p_buffer = buffer;
                encode_buffer_payload(p_buffer);
                asio::async_write(
                    tcp.ssl_socket, asio::buffer(p_buffer.data(), p_buffer.size()),
                    std::bind(&WebsocketClientSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
            }
            mutex_buffer.unlock();
        }

        void package_string(const std::string &str) {
            std::string payload;
            if (!split_buffer ||
                str.size() + get_frame_encode_size(str.size()) <= max_send_buffer_size) {
                sdata_frame.fin = true;
                payload = encode_string_payload(str);
                asio::async_write(
                    tcp.ssl_socket, asio::buffer(payload.data(), payload.size()),
                    std::bind(&WebsocketClientSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
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
                    tcp.ssl_socket, asio::buffer(payload, payload.size()),
                    std::bind(&WebsocketClientSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
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

        void package_buffer(const std::vector<uint8_t> &buffer) {
            std::vector<uint8_t> payload;
            if (!split_buffer || buffer.size() + get_frame_encode_size(buffer.size()) <=
                max_send_buffer_size) {
                sdata_frame.fin = true;
                payload = encode_buffer_payload(buffer);
                asio::async_write(
                    tcp.ssl_socket, asio::buffer(payload.data(), payload.size()),
                    std::bind(&WebsocketClientSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
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
                    tcp.ssl_socket, asio::buffer(payload, payload.size()),
                    std::bind(&WebsocketClientSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
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

        bool decode_payload(FWsMessage &data_frame) {
            if (asio::buffer_size(response_buffer.data()) < 2) return false;

            size_t size = asio::buffer_size(response_buffer.data());
            std::vector<uint8_t> encoded_buffer;
            encoded_buffer.resize(size);
            asio::buffer_copy(asio::buffer(encoded_buffer, encoded_buffer.size()),
                              response_buffer.data());

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

            padded_input += static_cast<char>(0x80);

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

        void consume_response_buffer() {
            const size_t res_size = response_buffer.size();
            if (res_size > 0)
                response_buffer.consume(response_buffer.size());
        }

        void run_context_thread() {
            error_code.clear();
            tcp.context.restart();
            tcp.resolver.async_resolve(
                host, service,
                std::bind(&WebsocketClientSsl::resolve, this,
                          asio::placeholders::error, asio::placeholders::endpoint));
            tcp.context.run();
            if (get_ssl_socket().next_layer().is_open() && !is_closing)
                close();
        }

        void resolve(const asio::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }

            // Attempt a connection to each endpoint in the list until we
            // successfully establish a connection.
            tcp.endpoints = endpoints;
            asio::async_connect(
                tcp.ssl_socket.lowest_layer(), tcp.endpoints,
                std::bind(&WebsocketClientSsl::conn, this, asio::placeholders::error));
        }

        void conn(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }

            // The connection was successful;
            tcp.ssl_socket.async_handshake(asio::ssl::stream_base::client,
                                           std::bind(&WebsocketClientSsl::ssl_handshake,
                                                     this, asio::placeholders::error));
        }

        void ssl_handshake(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            std::string request;
            request = "GET " + req_handshake.path + " HTTP/" + req_handshake.version + "\r\n";
            if (!req_handshake.headers.contains("Host"))
                request += "Host: " + host + ":" + service + "\r\n";
            if (!req_handshake.headers.empty()) {
                for (const auto &header: req_handshake.headers) {
                    request += header.first + ": " + header.second + "\r\n";
                }
            }
            request += "\r\n";
            asio::async_write(tcp.ssl_socket, asio::buffer(request.data(), request.size()),
                              std::bind(&WebsocketClientSsl::write_handshake, this,
                                        asio::placeholders::error,
                                        asio::placeholders::bytes_transferred));
        }

        void write_handshake(const asio::error_code &error, const size_t bytes_sent) {
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            // Read the response status line. The response_ streambuf will
            // automatically grow to accommodate the entire line. The growth may be
            // limited by passing a maximum size to the streambuf constructor.
            asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n",
                                   std::bind(&WebsocketClientSsl::read_handshake, this,
                                             asio::placeholders::error, asio::placeholders::bytes_transferred));
        }

        void read_handshake(const asio::error_code &error, size_t bytes_recvd) {
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            // Check that response is OK.
            std::istream response_stream(&response_buffer);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                std::lock_guard<std::mutex> lock(mutex_error);
                if (on_handshake_fail) on_handshake_fail(505, ResponseStatusCode.at(505));
                if (get_ssl_socket().next_layer().is_open() && !is_closing)
                    close();
                return;
            }
            if (status_code != 101) {
                std::lock_guard<std::mutex> lock(mutex_error);
                if (on_handshake_fail) on_handshake_fail(status_code,
                                                         ResponseStatusCode.contains(status_code)
                                                             ? ResponseStatusCode.at(status_code)
                                                             : "");
                if (get_ssl_socket().next_layer().is_open() && !is_closing)
                    close();
                return;
            }

            if (response_buffer.size() == 0) {
                if (on_connected) on_connected(Client::FResponse());
                asio::async_read(
                    tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
                    std::bind(&WebsocketClientSsl::read, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                return;
            }

            asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n\r\n",
                                   std::bind(&WebsocketClientSsl::read_headers,
                                             this, asio::placeholders::error));
        }

        void read_headers(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            Client::res_clear(res_handshake);
            std::istream response_stream(&response_buffer);
            std::string header;
            while (std::getline(response_stream, header) && header != "\r")
                Client::res_append_header(res_handshake, header);
            consume_response_buffer();

            if (res_handshake.headers.empty()) {
                if (on_handshake_fail) on_handshake_fail(-1, "Invalid header: Empty");
                if (get_ssl_socket().next_layer().is_open() && !is_closing)
                    close();
                return;
            }
            if (res_handshake.headers.at("Connection")[0] != "Upgrade" || res_handshake.headers.at("Upgrade")[0] !=
                "websocket") {
                if (on_handshake_fail) on_handshake_fail(-1, "Invalid header: Connection");
                if (get_ssl_socket().next_layer().is_open() && !is_closing)
                    close();
            }
            std::string protocol = req_handshake.headers.at("Sec-WebSocket-Protocol");
            std::string res_protocol = res_handshake.headers.at("Sec-WebSocket-Protocol")[0];
            if (protocol.find(res_protocol) == protocol.npos) {
                if (on_handshake_fail) on_handshake_fail(-1, "Invalid header: Sec-WebSocket-Protocol");
                if (get_ssl_socket().next_layer().is_open() && !is_closing)
                    close();
            }
            std::string accept_key = res_handshake.headers.at("Sec-WebSocket-Accept")[0];
            std::string encoded_key = generate_accept_key(req_handshake.headers.at("Sec-WebSocket-Key"));
            if (accept_key != encoded_key) {
                if (on_handshake_fail) on_handshake_fail(-1, "Invalid Sec-WebSocket-Accept");
                if (get_ssl_socket().next_layer().is_open() && !is_closing)
                    close();
            }

            if (on_connected) on_connected(res_handshake);
            asio::async_read(
                tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&WebsocketClientSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
        }

        void write(const asio::error_code &error, const std::size_t bytes_sent) {
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }

            if (on_message_sent) on_message_sent(bytes_sent);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd) {
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }

            FWsMessage rDataFrame;
            if (!decode_payload(rDataFrame)) {
                response_buffer.consume(response_buffer.size());
                asio::async_read(
                    tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
                    std::bind(&WebsocketClientSsl::read, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                return;
            }

            if (rDataFrame.data_frame.opcode == EOpcode::PING) {
                std::vector<uint8_t> pong_buffer;
                pong_buffer.resize(1);
                if (pong_buffer.back() != uint8_t('\0'))
                    pong_buffer.push_back(uint8_t('\0'));
                post_buffer(EOpcode::PONG, pong_buffer);
            } else if (rDataFrame.data_frame.opcode == EOpcode::PONG) {
                if (on_pong_received) on_pong_received();
            } else if (rDataFrame.data_frame.opcode == EOpcode::CONNECTION_CLOSE) {
                if (on_close_notify) on_close_notify();
            } else {
                rDataFrame.size = bytes_recvd;
                if (on_message_received) on_message_received(rDataFrame);
            }

            consume_response_buffer();
            asio::async_read(
                tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&WebsocketClientSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
        }
    };
#endif
} // namespace InternetProtocol
