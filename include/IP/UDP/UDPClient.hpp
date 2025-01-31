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
    class UDPClient {
    public:
        ~UDPClient() {
            udp.resolver.cancel();
            if (get_socket().is_open()) close();
        }

        /*HOST | LOCAL*/
        void set_host(const std::string &ip = "localhost", const std::string &port = "3000",
                      const EProtocolType protocol = EProtocolType::V4) {
            host = ip;
            service = port;
            protocol_type = protocol;
        }

        asio::ip::udp::socket &get_socket() { return udp.socket; }

        /*SETTINGS*/
        void set_max_send_buffer_size(int value = 1024) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_max_receive_buffer_size(int value = 1024) { max_receive_buffer_size = value; }
        int get_max_receive_buffer_size() const { return max_receive_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*MESSAGE*/
        bool send_str(const std::string &message) {
            if (!get_socket().is_open() || message.empty())
                return false;

            asio::post(thread_pool, std::bind(&UDPClient::package_string, this, message));
            return true;
        }

        bool send_buffer(const std::vector<uint8_t> &buffer) {
            if (!get_socket().is_open() || buffer.empty())
                return false;

            asio::post(thread_pool, std::bind(&UDPClient::package_buffer, this, buffer));
            return true;
        }

        /*CONNECTION*/
        bool connect() {
            if (get_socket().is_open())
                return false;

            asio::post(thread_pool, std::bind(&UDPClient::run_context_thread, this));
            return true;
        }

        void close() {
            is_closing = true;
            udp.context.stop();
            if (udp.socket.is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                udp.socket.shutdown(asio::ip::udp::socket::shutdown_both, error_code);
                if (error_code && on_error) {
                    on_error(error_code);
                }
                udp.socket.close(error_code);
                if (error_code && on_error) {
                    on_error(error_code);
                }
            }
            udp.context.restart();
            if (on_close)
                on_close();
            is_closing = false;
        }

        /*ERRORS*/
        asio::error_code get_error_code() const { return error_code; }

        /*EVENTS*/
        std::function<void()> on_connected;
        std::function<void(const size_t, const size_t)> on_bytes_transfered;
        std::function<void(const asio::error_code &)> on_message_sent;
        std::function<void(const FUdpMessage)> on_message_received;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        std::mutex mutex_error;
        bool is_closing = false;
        Client::FAsioUdp udp;
        asio::error_code error_code;
        std::string host = "localhost";
        std::string service = "3000";
        EProtocolType protocol_type = EProtocolType::V4;
        bool split_buffer = true;
        int max_send_buffer_size = 1024;
        int max_receive_buffer_size = 1024;
        FUdpMessage rbuffer;

        void package_string(const std::string &str) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || str.size() <= max_send_buffer_size) {
                udp.socket.async_send_to(asio::buffer(str.data(), str.size()), udp.endpoints,
                                         std::bind(&UDPClient::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                return;
            }

            size_t string_offset = 0;
            const size_t max_size = max_send_buffer_size;
            while (string_offset < str.size()) {
                size_t package_size = std::min(max_size, str.size() - string_offset);
                std::string strshrink(str.begin() + string_offset,
                                      str.begin() + string_offset + package_size);
                udp.socket.async_send_to(asio::buffer(strshrink.data(), strshrink.size()), udp.endpoints,
                                         std::bind(&UDPClient::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                string_offset += package_size;
            }
        }

        void package_buffer(const std::vector<uint8_t> &buffer) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || buffer.size() <= max_send_buffer_size) {
                udp.socket.async_send_to(asio::buffer(buffer.data(), buffer.size()), udp.endpoints,
                                         std::bind(&UDPClient::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = max_send_buffer_size;
            while (buffer_offset < buffer.size()) {
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                std::vector<uint8_t> sbuffer(buffer.begin() + buffer_offset,
                                             buffer.begin() + buffer_offset + package_size);
                udp.socket.async_send_to(asio::buffer(sbuffer.data(), sbuffer.size()), udp.endpoints,
                                         std::bind(&UDPClient::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                buffer_offset += package_size;
            }
        }

        void consume_receive_buffer() {
            rbuffer.size = 0;
            if (!rbuffer.raw_data.empty()) {
                rbuffer.raw_data.clear();
                rbuffer.raw_data.shrink_to_fit();
            }
            if (rbuffer.raw_data.size() != max_receive_buffer_size)
                rbuffer.raw_data.resize(max_send_buffer_size);
        }

        /*ASYNC HANDLER FUNCTIONS*/
        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            error_code.clear();
            udp.resolver.async_resolve(protocol_type == EProtocolType::V4 ? asio::ip::udp::v4() : asio::ip::udp::v6(),
                                       host, service,
                                       std::bind(&UDPClient::resolve, this, asio::placeholders::error,
                                                 asio::placeholders::results));
            udp.context.run();
            if (get_socket().is_open() && !is_closing)
                close();
        }

        void resolve(const asio::error_code &error, const asio::ip::udp::resolver::results_type &results) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            udp.endpoints = results.begin()->endpoint();
            udp.socket.async_connect(udp.endpoints,
                                     std::bind(&UDPClient::conn, this, asio::placeholders::error));
        }

        void conn(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            if (rbuffer.raw_data.size() != max_receive_buffer_size)
                consume_receive_buffer();
            udp.socket.async_receive_from(asio::buffer(rbuffer.raw_data, rbuffer.raw_data.size()), udp.endpoints,
                                          std::bind(&UDPClient::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));

            if (on_connected)
                on_connected();
        }

        void send_to(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_message_sent)
                    on_message_sent(error);
                return;
            }

            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (on_message_sent)
                on_message_sent(error);
        }

        void receive_from(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }

            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            rbuffer.size = bytes_recvd;
            rbuffer.raw_data.resize(bytes_recvd);
            if (on_message_received)
                on_message_received(rbuffer);

            consume_receive_buffer();
            udp.socket.async_receive_from(asio::buffer(rbuffer.raw_data, rbuffer.raw_data.size()), udp.endpoints,
                                          std::bind(&UDPClient::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));
        }
    };
}
