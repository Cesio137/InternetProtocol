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
        ~WebsocketClient() {
            should_stop_context = true;
            tcp.resolver.cancel();
            if (is_connected()) close();
            thread_pool->stop();
            consume_response_buffer();
        }

        /*HOST*/
        void set_host(const std::string &url = "localhost", const std::string &port = "80") {
            host = url;
            service = port;
        }

        std::string get_local_adress() const {
            return tcp.socket.local_endpoint().address().to_string();
        }

        int get_local_port() const {
            return tcp.socket.local_endpoint().port();
        }

        std::string get_remote_adress() const {
            return tcp.socket.remote_endpoint().address().to_string();
        }

        int get_remote_port() const {
            return tcp.socket.remote_endpoint().port();
        }

        /*SETTINGS*/
        void set_timeout(uint8_t value = 3) { timeout = value; }
        uint8_t get_timeout() const { return timeout; }
        void set_max_attemp(uint8_t value = 3) { max_attemp = value; }
        uint8_t get_max_attemp() const { return timeout; }
        void set_max_send_buffer_size(int value = 1400) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*HANDSHAKE*/
        void set_path(const std::string &value = "chat") { handshake.path = value; }
        std::string ge_pPath() const { return handshake.path; }

        void set_version(const std::string &value = "1.1") {
            handshake.version = value;
        }

        std::string get_version() const { return handshake.version; }

        void set_key(const std::string &value = "dGhlIHNhbXBsZSBub25jZQ==") {
            handshake.Sec_WebSocket_Key = value;
        }

        std::string get_key() const { return handshake.Sec_WebSocket_Key; }

        void set_origin(const std::string &value = "client") {
            handshake.origin = value;
        }

        std::string get_origin() const { return handshake.origin; }

        void set_sec_protocol(const std::string &value = "chat, superchat") {
            handshake.Sec_WebSocket_Protocol = value;
        }

        std::string get_sec_protocol() const {
            return handshake.Sec_WebSocket_Protocol;
        }

        void set_sec_version(const std::string &value = "13") {
            handshake.Sec_Websocket_Version = value;
        }

        std::string getSecVersion() const { return handshake.Sec_Websocket_Version; }

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
        bool send(const std::string &message) {
            if (!thread_pool && !is_connected() && !message.empty()) return false;

            asio::post(*thread_pool, std::bind(&WebsocketClient::post_string, this, message));
            return true;
        }

        bool send_buffer(const std::vector<std::byte> &buffer) {
            if (!thread_pool && !is_connected() && !buffer.empty()) return false;

            asio::post(*thread_pool, std::bind(&WebsocketClient::post_buffer, this,
                                               EOpcode::BINARY_FRAME, buffer));
            return true;
        }

        bool send_ping() {
            if (!thread_pool && !is_connected()) return false;

            std::vector<std::byte> ping_buffer = {
                std::byte('p'), std::byte('i'), std::byte('n'), std::byte('g'), std::byte('\0')
            };
            asio::post(*thread_pool, std::bind(&WebsocketClient::post_buffer, this,
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
            if (!thread_pool && is_connected()) return false;

            asio::post(*thread_pool, std::bind(&WebsocketClient::run_context_thread, this));
            return true;
        }

        bool is_connected() const { return tcp.socket.is_open(); }

        void close() {
            asio::error_code ec_shutdown;
            asio::error_code ec_close;
            tcp.context.stop();
            tcp.socket.shutdown(asio::ip::udp::socket::shutdown_both, ec_shutdown);
            tcp.socket.close(ec_close);
            if (should_stop_context) return;
            if (ec_shutdown && on_error) {
                on_error(ec_shutdown);
                return;
            }

            if (ec_close && on_error) {
                on_error(ec_close);
                return;
            }
            if (on_close)
                on_close();
        }

        /*EVENTS*/
        std::function<void()> on_connected;
        std::function<void(const int)> on_connection_retry;
        std::function<void()> on_close;
        std::function<void()> on_close_notify;
        std::function<void(const size_t)> on_message_sent;
        std::function<void(const FWsMessage)> on_message_received;
        std::function<void()> on_pong_received;
        std::function<void(const int, const std::string &)> on_handshake_fail;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::unique_ptr<asio::thread_pool> thread_pool =
                std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        bool should_stop_context = false;
        std::string host = "localhost";
        std::string service;
        uint8_t timeout = 3;
        uint8_t max_attemp = 3;
        bool split_buffer = false;
        int max_send_buffer_size = 1400;
        FAsioTcp tcp;
        asio::streambuf request_buffer;
        asio::streambuf response_buffer;
        FHandShake handshake;
        FDataFrame sdata_frame;

        void post_string(const std::string &str) {
            mutex_buffer.lock();
            sdata_frame.opcode = EOpcode::TEXT_FRAME;
            package_string(str);
            mutex_buffer.unlock();
        }

        void post_buffer(EOpcode opcode, const std::vector<std::byte> &buffer) {
            mutex_buffer.lock();
            sdata_frame.opcode = opcode;
            if (opcode == EOpcode::BINARY_FRAME) {
                package_buffer(buffer);
            } else if (opcode == EOpcode::PING || opcode == EOpcode::PONG) {
                std::vector<std::byte> p_buffer = buffer;
                encode_buffer_payload(p_buffer);
                asio::async_write(
                    tcp.socket, asio::buffer(p_buffer.data(), p_buffer.size()),
                    std::bind(&WebsocketClient::write, this, asio::placeholders::error,
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

            std::array<std::byte, 4> masking_key;
            if (sdata_frame.mask) {
                masking_key = mask_gen();
                for (std::byte key: masking_key)
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

        void package_buffer(const std::vector<std::byte> &buffer) {
            std::vector<std::byte> payload;
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

        std::vector<std::byte> encode_buffer_payload(
            const std::vector<std::byte> &payload) {
            std::vector<std::byte> buffer;
            uint64_t payload_length = payload.size();

            // FIN, RSV, Opcode
            std::byte byte1 = std::byte(sdata_frame.fin ? 0x80 : 0x00);
            byte1 |= std::byte(sdata_frame.rsv1 ? (uint8_t) ERSV::RSV1 : 0x00);
            byte1 |= std::byte(sdata_frame.rsv2 ? (uint8_t) ERSV::RSV2 : 0x00);
            byte1 |= std::byte(sdata_frame.rsv3 ? (uint8_t) ERSV::RSV3 : 0x00);
            byte1 |= std::byte((uint8_t) sdata_frame.opcode & 0x0F);
            buffer.push_back(byte1);

            // Mask and payload size
            std::byte byte2 = std::byte(sdata_frame.mask ? 0x80 : 0x00);
            if (payload_length <= 125) {
                byte2 |= std::byte(payload_length);
                buffer.push_back(byte2);
            } else if (payload_length <= 65535) {
                byte2 |= std::byte(126);
                buffer.push_back(byte2);
                buffer.push_back(std::byte((payload_length >> 8) & 0xFF));
                buffer.push_back(std::byte(payload_length & 0xFF));
            } else {
                byte2 |= std::byte(127);
                buffer.push_back(byte2);
                for (int i = 7; i >= 0; --i) {
                    buffer.push_back(std::byte((payload_length >> (8 * i)) & 0xFF));
                }
            }

            std::array<std::byte, 4> masking_key;
            if (sdata_frame.mask) {
                masking_key = mask_gen();
                for (std::byte key: masking_key) buffer.push_back(key);
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

        std::array<std::byte, 4> mask_gen() {
            std::array<std::byte, 4> maskKey;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);

            for (std::byte &byte: maskKey) {
                byte = std::byte(dis(gen));
            }

            return maskKey;
        }

        bool decode_payload(FWsMessage &data_frame) {
            if (asio::buffer_size(response_buffer.data()) < 2) return false;

            size_t size = asio::buffer_size(response_buffer.data());
            std::vector<std::byte> encoded_buffer;
            encoded_buffer.resize(size);
            asio::buffer_copy(asio::buffer(encoded_buffer, encoded_buffer.size()),
                              response_buffer.data());

            size_t pos = 0;
            // FIN, RSV, Opcode
            std::byte byte1 = encoded_buffer[pos++];
            data_frame.data_frame.fin = (uint8_t) byte1 & 0x80;
            data_frame.data_frame.rsv1 = (uint8_t) byte1 & 0x80;
            data_frame.data_frame.rsv2 = (uint8_t) byte1 & 0x40;
            data_frame.data_frame.rsv3 = (uint8_t) byte1 & 0x10;
            data_frame.data_frame.opcode = (EOpcode) ((uint8_t) byte1 & 0x0F);

            // Mask and payload length
            std::byte byte2 = encoded_buffer[pos++];
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
                        (std::byte(payload_length) << 8) | encoded_buffer[pos + i]);
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

        void consume_response_buffer() {
            request_buffer.consume(request_buffer.size());
            response_buffer.consume(response_buffer.size());
        }

        void run_context_thread() {
            mutex_io.lock();
            while (tcp.attemps_fail <= max_attemp && !should_stop_context) {
                if (on_connection_retry && tcp.attemps_fail > 0)
                    on_connection_retry(tcp.attemps_fail);
                tcp.error_code.clear();
                tcp.resolver.async_resolve(
                    host, service,
                    std::bind(&WebsocketClient::resolve, this, asio::placeholders::error,
                              asio::placeholders::endpoint));
                tcp.context.run();
                tcp.context.restart();
                if (!tcp.error_code) break;
                tcp.attemps_fail++;
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
            }
            consume_response_buffer();
            tcp.attemps_fail = 0;
            mutex_io.unlock();
        }

        void resolve(const asio::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }
            tcp.endpoints = endpoints;
            asio::async_connect(
                tcp.socket, tcp.endpoints,
                std::bind(&WebsocketClient::conn, this, asio::placeholders::error));
        }

        void conn(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }
            std::string request;
            request = "GET /" + handshake.path + " HTTP/" + handshake.version + "\r\n";
            request += "Host: " + host + "\r\n";
            request += "Upgrade: websocket\r\n";
            request += "Connection: Upgrade\r\n";
            request += "Sec-WebSocket-Key: " + handshake.Sec_WebSocket_Key + "\r\n";
            request += "Origin: " + handshake.origin + "\r\n";
            request +=
                    "Sec-WebSocket-Protocol: " + handshake.Sec_WebSocket_Protocol + "\r\n";
            request +=
                    "Sec-WebSocket-Version: " + handshake.Sec_Websocket_Version + "\r\n";
            request += "\r\n";
            std::ostream request_stream(&request_buffer);
            request_stream << request;
            asio::async_write(tcp.socket, request_buffer,
                              std::bind(&WebsocketClient::write_handshake, this,
                                        asio::placeholders::error,
                                        asio::placeholders::bytes_transferred));
        }

        void write_handshake(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }
            asio::async_read_until(tcp.socket, response_buffer, "\r\n",
                                   std::bind(&WebsocketClient::read_handshake, this,
                                             asio::placeholders::error, bytes_sent,
                                             asio::placeholders::bytes_transferred));
        }

        void read_handshake(const asio::error_code &error, const size_t bytes_sent,
                            size_t bytes_recvd) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
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
                if (on_handshake_fail) on_handshake_fail(-1, "Invalid response.");
                close();
                return;
            }
            if (status_code != 101) {
                if (on_handshake_fail) on_handshake_fail(status_code, "Invalid status code.");
                close();
                return;
            }

            asio::async_read_until(tcp.socket, response_buffer, "\r\n\r\n",
                                   std::bind(&WebsocketClient::consume_header_buffer,
                                             this, asio::placeholders::error));
        }

        void consume_header_buffer(const asio::error_code &error) {
            consume_response_buffer();
            asio::async_read(
                tcp.socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&WebsocketClient::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
            if (on_connected) on_connected();
        }

        void write(const asio::error_code &error, const std::size_t bytes_sent) {
            if (error) {
                if (on_error) on_error(error);
                return;
            }
            if (on_message_sent) on_message_sent(bytes_sent);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                if (on_error) on_error(error);
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
                std::vector<std::byte> pong_buffer = {
                    std::byte('p'), std::byte('o'), std::byte('n'), std::byte('g'), std::byte('\0')
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
        ~WebsocketClientSsl() {
            should_stop_context = true;
            tcp.resolver.cancel();
            if (is_connected()) close();
            thread_pool->stop();
            consume_response_buffer();
        }

        /*HOST*/
        void set_host(const std::string &url = "localhost", const std::string &port = "80") {
            host = url;
            service = port;
        }

        std::string get_local_adress() const {
            return tcp.ssl_socket.lowest_layer()
                    .local_endpoint()
                    .address()
                    .to_string();
        }

        int get_local_port() const {
            return tcp.ssl_socket.lowest_layer()
                    .local_endpoint()
                    .port();
        }

        std::string get_remote_adress() const {
            return tcp.ssl_socket.lowest_layer()
                    .remote_endpoint()
                    .address()
                    .to_string();
        }

        int get_remote_port() const {
            return tcp.ssl_socket.lowest_layer().remote_endpoint().port();
        }

        /*SETTINGS*/
        void set_timeout(uint8_t value = 3) { timeout = value; }
        uint8_t get_timeout() const { return timeout; }
        void set_max_attemp(uint8_t value = 3) { max_attemp = value; }
        uint8_t get_max_attemp() const { return timeout; }
        void set_max_send_buffer_size(int value = 1400) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*HANDSHAKE*/
        void set_path(const std::string &value = "chat") { handshake.path = value; }
        std::string ge_pPath() const { return handshake.path; }

        void set_version(const std::string &value = "1.1") {
            handshake.version = value;
        }

        std::string get_version() const { return handshake.version; }

        void set_key(const std::string &value = "dGhlIHNhbXBsZSBub25jZQ==") {
            handshake.Sec_WebSocket_Key = value;
        }

        std::string get_key() const { return handshake.Sec_WebSocket_Key; }

        void set_origin(const std::string &value = "client") {
            handshake.origin = value;
        }

        std::string get_origin() const { return handshake.origin; }

        void set_sec_protocol(const std::string &value = "chat, superchat") {
            handshake.Sec_WebSocket_Protocol = value;
        }

        std::string get_sec_protocol() const {
            return handshake.Sec_WebSocket_Protocol;
        }

        void set_sec_version(const std::string &value = "13") {
            handshake.Sec_Websocket_Version = value;
        }

        std::string getSecVersion() const { return handshake.Sec_Websocket_Version; }

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
            asio::error_code ec;
            const asio::const_buffer buffer(key_data.data(), key_data.size());
            tcp.ssl_context.use_private_key(buffer, asio::ssl::context::pem, ec);
            if (ec) {
                if (on_error) on_error(ec);
                return false;
            }
            return true;
        }

        bool load_private_key_file(const std::string &filename) {
            if (filename.empty()) return false;
            asio::error_code ec;
            tcp.ssl_context.use_private_key_file(filename, asio::ssl::context::pem, ec);
            if (ec) {
                if (on_error) on_error(ec);
                return false;
            }
            return true;
        }

        bool load_certificate_data(const std::string &cert_data) {
            if (cert_data.empty()) return false;
            asio::error_code ec;
            const asio::const_buffer buffer(cert_data.data(), cert_data.size());
            tcp.ssl_context.use_certificate(buffer, asio::ssl::context::pem, ec);
            if (ec) {
                if (on_error) on_error(ec);
                return false;
            }
            return true;
        }

        bool load_certificate_file(const std::string &filename) {
            if (filename.empty()) return false;
            asio::error_code ec;
            tcp.ssl_context.use_certificate_file(filename, asio::ssl::context::pem, ec);
            if (ec) {
                if (on_error) on_error(ec);
                return false;
            }
            return true;
        }

        bool load_certificate_chain_data(const std::string &cert_chain_data) {
            if (cert_chain_data.empty()) return false;
            asio::error_code ec;
            const asio::const_buffer buffer(cert_chain_data.data(), cert_chain_data.size());
            tcp.ssl_context.use_certificate_chain(buffer, ec);
            if (ec) {
                if (on_error) on_error(ec);
                return false;
            }
            return true;
        }

        bool load_certificate_chain_file(const std::string &filename) {
            if (filename.empty()) return false;
            asio::error_code ec;
            tcp.ssl_context.use_certificate_chain_file(filename, ec);
            if (ec) {
                if (on_error) on_error(ec);
                return false;
            }
            return true;
        }

        bool load_verify_file(const std::string &filename) {
            if (filename.empty()) return false;
            asio::error_code ec;
            tcp.ssl_context.load_verify_file(filename, ec);
            if (ec) {
                if (on_error) on_error(ec);
                return false;
            }
            return true;
        }

        /*MESSAGE*/
        bool send(const std::string &message) {
            if (!thread_pool && !is_connected() && !message.empty()) return false;

            asio::post(*thread_pool,
                       std::bind(&WebsocketClientSsl::post_string, this, message));
            return true;
        }

        bool send_buffer(const std::vector<std::byte> &buffer) {
            if (!thread_pool && !is_connected() && !buffer.empty()) return false;

            asio::post(*thread_pool, std::bind(&WebsocketClientSsl::post_buffer, this,
                                               EOpcode::BINARY_FRAME, buffer));
            return true;
        }

        bool send_ping() {
            if (!thread_pool && !is_connected()) return false;

            std::vector<std::byte> ping_buffer;
            ping_buffer.push_back(std::byte('\0'));
            asio::post(*thread_pool, std::bind(&WebsocketClientSsl::post_buffer, this,
                                               EOpcode::PING, ping_buffer));
            return true;
        }

        bool async_read() {
            if (!is_connected()) return false;

            asio::async_read(
                tcp.ssl_socket, response_buffer, asio::transfer_at_least(2),
                std::bind(&WebsocketClientSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
            return true;
        }

        /*CONNECTION*/
        bool connect() {
            if (!thread_pool && is_connected()) return false;

            asio::post(*thread_pool, std::bind(&WebsocketClientSsl::run_context_thread, this));
            return true;
        }

        bool is_connected() const { return tcp.ssl_socket.lowest_layer().is_open(); }

        void close() {
            tcp.context.stop();
            asio::error_code ec_shutdown;
            asio::error_code ec_close;
            tcp.ssl_socket.shutdown(ec_shutdown);
            tcp.ssl_socket.lowest_layer().close(ec_close);
            if (should_stop_context) return;
            if (ec_shutdown && on_error)
                on_error(ec_shutdown);
            tcp.ssl_socket.lowest_layer().close(ec_close);
            if (ec_close && on_error)
                on_error(ec_close);
            if (on_close) on_close();
        }

        /*ERRORS*/
        asio::error_code get_error_code() const { return tcp.error_code; }

        /*EVENTS*/
        std::function<void()> on_connected;
        std::function<void(const int)> on_connection_retry;
        std::function<void()> on_close;
        std::function<void()> on_close_notify;
        std::function<void(const size_t)> on_message_sent;
        std::function<void(const FWsMessage)> on_message_received;
        std::function<void()> on_pong_received;
        std::function<void(const int, const std::string &)> on_handshake_fail;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::unique_ptr<asio::thread_pool> thread_pool =
                std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        bool should_stop_context = false;
        std::string host = "localhost";
        std::string service;
        uint8_t timeout = 3;
        uint8_t max_attemp = 3;
        bool split_buffer = false;
        int max_send_buffer_size = 1400;
        FAsioTcpSsl tcp;
        asio::streambuf request_buffer;
        asio::streambuf response_buffer;
        FHandShake handshake;
        FDataFrame sdata_frame;

        void post_string(const std::string &str) {
            mutex_buffer.lock();
            sdata_frame.opcode = EOpcode::TEXT_FRAME;
            package_string(str);
            mutex_buffer.unlock();
        }

        void post_buffer(EOpcode opcode, const std::vector<std::byte> &buffer) {
            mutex_buffer.lock();
            sdata_frame.opcode = opcode;
            if (opcode == EOpcode::BINARY_FRAME) {
                package_buffer(buffer);
            } else if (opcode == EOpcode::PING || opcode == EOpcode::PONG) {
                std::vector<std::byte> p_buffer = buffer;
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

            std::array<std::byte, 4> masking_key;
            if (sdata_frame.mask) {
                masking_key = mask_gen();
                for (std::byte key: masking_key)
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

        void package_buffer(const std::vector<std::byte> &buffer) {
            std::vector<std::byte> payload;
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

        std::vector<std::byte> encode_buffer_payload(
            const std::vector<std::byte> &payload) {
            std::vector<std::byte> buffer;
            uint64_t payload_length = payload.size();

            // FIN, RSV, Opcode
            std::byte byte1 = std::byte(sdata_frame.fin ? 0x80 : 0x00);
            byte1 |= std::byte(sdata_frame.rsv1 ? (uint8_t) ERSV::RSV1 : 0x00);
            byte1 |= std::byte(sdata_frame.rsv2 ? (uint8_t) ERSV::RSV2 : 0x00);
            byte1 |= std::byte(sdata_frame.rsv3 ? (uint8_t) ERSV::RSV3 : 0x00);
            byte1 |= std::byte((uint8_t) sdata_frame.opcode & 0x0F);
            buffer.push_back(byte1);

            // Mask and payload size
            std::byte byte2 = std::byte(sdata_frame.mask ? 0x80 : 0x00);
            if (payload_length <= 125) {
                byte2 |= std::byte(payload_length);
                buffer.push_back(byte2);
            } else if (payload_length <= 65535) {
                byte2 |= std::byte(126);
                buffer.push_back(byte2);
                buffer.push_back(std::byte((payload_length >> 8) & 0xFF));
                buffer.push_back(std::byte(payload_length & 0xFF));
            } else {
                byte2 |= std::byte(127);
                buffer.push_back(byte2);
                for (int i = 7; i >= 0; --i) {
                    buffer.push_back(std::byte((payload_length >> (8 * i)) & 0xFF));
                }
            }

            std::array<std::byte, 4> masking_key;
            if (sdata_frame.mask) {
                masking_key = mask_gen();
                for (std::byte key: masking_key) buffer.push_back(key);
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

        std::array<std::byte, 4> mask_gen() {
            std::array<std::byte, 4> maskKey;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);

            for (std::byte &byte: maskKey) {
                byte = std::byte(dis(gen));
            }

            return maskKey;
        }

        bool decode_payload(FWsMessage &data_frame) {
            if (asio::buffer_size(response_buffer.data()) < 2) return false;

            size_t size = asio::buffer_size(response_buffer.data());
            std::vector<std::byte> encoded_buffer;
            encoded_buffer.resize(size);
            asio::buffer_copy(asio::buffer(encoded_buffer, encoded_buffer.size()),
                              response_buffer.data());

            size_t pos = 0;
            // FIN, RSV, Opcode
            std::byte byte1 = encoded_buffer[pos++];
            data_frame.data_frame.fin = (uint8_t) byte1 & 0x80;
            data_frame.data_frame.rsv1 = (uint8_t) byte1 & 0x80;
            data_frame.data_frame.rsv2 = (uint8_t) byte1 & 0x40;
            data_frame.data_frame.rsv3 = (uint8_t) byte1 & 0x10;
            data_frame.data_frame.opcode = (EOpcode) ((uint8_t) byte1 & 0x0F);

            // Mask and payload length
            std::byte byte2 = encoded_buffer[pos++];
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
                        (std::byte(payload_length) << 8) | encoded_buffer[pos + i]);
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

        void consume_response_buffer() {
            request_buffer.consume(request_buffer.size());
            response_buffer.consume(response_buffer.size());
        }

        void run_context_thread() {
            mutex_io.lock();
            while (tcp.attemps_fail <= max_attemp && !should_stop_context) {
                if (on_connection_retry && tcp.attemps_fail > 0)
                    on_connection_retry(tcp.attemps_fail);
                tcp.error_code.clear();
                tcp.context.restart();
                tcp.resolver.async_resolve(
                    host, service,
                    std::bind(&WebsocketClientSsl::resolve, this,
                              asio::placeholders::error, asio::placeholders::endpoint));
                tcp.context.run();
                if (!tcp.error_code) break;
                tcp.attemps_fail++;
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
            }
            consume_response_buffer();
            tcp.attemps_fail = 0;
            mutex_io.unlock();
        }

        void resolve(const asio::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
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
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }

            // The connection was successful;
            tcp.ssl_socket.async_handshake(asio::ssl::stream_base::client,
                                           std::bind(&WebsocketClientSsl::ssl_handshake,
                                                     this, asio::placeholders::error));
        }

        void ssl_handshake(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }
            std::string request;
            request = "GET /" + handshake.path + " HTTP/" + handshake.version + "\r\n";
            request += "Host: " + host + "\r\n";
            request += "Upgrade: websocket\r\n";
            request += "Connection: Upgrade\r\n";
            request += "Sec-WebSocket-Key: " + handshake.Sec_WebSocket_Key + "\r\n";
            request += "Origin: " + handshake.origin + "\r\n";
            request +=
                    "Sec-WebSocket-Protocol: " + handshake.Sec_WebSocket_Protocol + "\r\n";
            request +=
                    "Sec-WebSocket-Version: " + handshake.Sec_Websocket_Version + "\r\n";
            request += "\r\n";
            std::ostream request_stream(&request_buffer);
            request_stream << request;
            asio::async_write(tcp.ssl_socket, request_buffer,
                              std::bind(&WebsocketClientSsl::write_handshake, this,
                                        asio::placeholders::error,
                                        asio::placeholders::bytes_transferred));
        }

        void write_handshake(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }
            // Read the response status line. The response_ streambuf will
            // automatically grow to accommodate the entire line. The growth may be
            // limited by passing a maximum size to the streambuf constructor.
            asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n",
                                   std::bind(&WebsocketClientSsl::read_handshake, this,
                                             asio::placeholders::error, bytes_sent,
                                             asio::placeholders::bytes_transferred));
        }

        void read_handshake(const asio::error_code &error, const size_t bytes_sent,
                            size_t bytes_recvd) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
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
                if (on_handshake_fail) on_handshake_fail(-1, "Invalid response.");
                close();
                return;
            }
            if (status_code != 101) {
                if (on_handshake_fail) on_handshake_fail(status_code, "Invalid status code.");
                close();
                return;
            }

            asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n\r\n",
                                   std::bind(&WebsocketClientSsl::consume_header_buffer,
                                             this, asio::placeholders::error));
        }

        void consume_header_buffer(const asio::error_code &error) {
            consume_response_buffer();
            asio::async_read(
                tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&WebsocketClientSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));

            // The connection was successful
            if (on_connected) on_connected();
        }

        void write(const asio::error_code &error, const std::size_t bytes_sent) {
            if (error) {
                if (on_error) on_error(error);

                return;
            }

            if (on_message_sent) on_message_sent(bytes_sent);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                if (on_error) on_error(error);
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
                std::vector<std::byte> pong_buffer;
                pong_buffer.resize(1);
                if (pong_buffer.back() != std::byte('\0'))
                    pong_buffer.push_back(std::byte('\0'));
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
