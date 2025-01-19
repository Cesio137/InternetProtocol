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
    class UDPServer {
    public:
        ~UDPServer() {
            if (udp.socket.is_open()) close();
        }

        /*HOST | LOCAL*/
        void set_socket(const EProtocolType protocol, const uint16_t port) {
            protocol_type = protocol;
            udp_port = port;
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
        bool send_str_to(const std::string &message, const asio::ip::udp::endpoint &endpoint) {
            if (!get_socket().is_open() || message.empty())
                return false;

            asio::post(thread_pool, std::bind(&UDPServer::package_string, this, message, endpoint));
            return true;
        }

        bool send_buffer_to(const std::vector<uint8_t> &buffer, const asio::ip::udp::endpoint &endpoint) {
            if (!get_socket().is_open() || buffer.empty())
                return false;

            asio::post(thread_pool, std::bind(&UDPServer::package_buffer, this, buffer, endpoint));
            return true;
        }

        /*CONNECTION*/
        bool open() {
            if (udp.socket.is_open())
                return false;

            udp.socket.open(protocol_type == EProtocolType::V4 ? asio::ip::udp::v4() : asio::ip::udp::v6(),
                            error_code);
            if (error_code) {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }
            udp.socket.bind(
                asio::ip::udp::endpoint(protocol_type == EProtocolType::V4
                                            ? asio::ip::udp::v4()
                                            : asio::ip::udp::v6(), udp_port), error_code);
            if (error_code) {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }
            if (on_open)
                on_open();

            asio::post(thread_pool, std::bind(&UDPServer::run_context_thread, this));
            return true;
        }

        void close() {
            is_closing = true;
            udp.context.stop();
            if (udp.socket.is_open()) {
                udp.socket.shutdown(asio::ip::udp::socket::shutdown_both, error_code);
                if (error_code && on_error) {
                    std::lock_guard<std::mutex> guard(mutex_error);
                    if (on_error) on_error(error_code);
                }
                udp.socket.close(error_code);
                if (error_code && on_error) {
                    std::lock_guard<std::mutex> guard(mutex_error);
                    if (on_error) on_error(error_code);
                }
            }
            udp.context.restart();
            if (on_close)
                on_close();
            is_closing = true;
        }

        /*ERRORS*/
        asio::error_code get_error_code() const { return error_code; }

        /*EVENTS*/
        std::function<void()> on_open;
        std::function<void(const size_t, const size_t)> on_bytes_transfered;
        std::function<void(const asio::ip::udp::endpoint &)> on_message_sent;
        std::function<void(const FUdpMessage, const asio::ip::udp::endpoint &)> on_message_received;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &)> on_socket_error;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        std::mutex mutex_error;
        bool is_closing = false;
        Server::FAsioUdp udp;
        asio::error_code error_code;
        EProtocolType protocol_type = EProtocolType::V4;
        uint16_t udp_port = 3000;
        bool split_buffer = true;
        int max_send_buffer_size = 1024;
        int max_receive_buffer_size = 1024;
        FUdpMessage rbuffer;

        void package_string(const std::string &str, const asio::ip::udp::endpoint &endpoint) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || str.size() <= max_send_buffer_size) {
                udp.socket.async_send_to(asio::buffer(str.data(), str.size()), endpoint,
                                         std::bind(&UDPServer::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred, endpoint));
                return;
            }

            size_t string_offset = 0;
            const size_t max_size = max_send_buffer_size;
            while (string_offset < str.size()) {
                size_t package_size = std::min(max_size, str.size() - string_offset);
                std::string strshrink(str.begin() + string_offset,
                                      str.begin() + string_offset + package_size);
                udp.socket.async_send_to(asio::buffer(strshrink.data(), strshrink.size()), endpoint,
                                         std::bind(&UDPServer::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred, endpoint));
                string_offset += package_size;
            }
        }

        void package_buffer(const std::vector<uint8_t> &buffer, const asio::ip::udp::endpoint &endpoint) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || buffer.size() <= max_send_buffer_size) {
                udp.socket.async_send_to(asio::buffer(buffer.data(), buffer.size()), endpoint,
                                         std::bind(&UDPServer::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred, endpoint));
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = max_send_buffer_size;
            while (buffer_offset < buffer.size()) {
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                std::vector<uint8_t> sbuffer(buffer.begin() + buffer_offset,
                                               buffer.begin() + buffer_offset + package_size);
                udp.socket.async_send_to(asio::buffer(sbuffer.data(), sbuffer.size()), endpoint,
                                         std::bind(&UDPServer::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred, endpoint));
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
                rbuffer.raw_data.resize(max_receive_buffer_size);
        }

        /*ASYNC HANDLER FUNCTIONS*/
        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            error_code.clear();
            if (rbuffer.raw_data.size() != max_receive_buffer_size)
                consume_receive_buffer();
            udp.socket.async_receive_from(asio::buffer(rbuffer.raw_data, rbuffer.raw_data.size()),
                                          udp.remote_endpoint,
                                          std::bind(&UDPServer::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));
            udp.context.run();
            if (get_socket().is_open() && !is_closing)
                close();
        }

        void send_to(const asio::error_code &error, const size_t bytes_sent, const asio::ip::udp::endpoint &endpoint) {
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            if (on_message_sent)
                on_message_sent(endpoint);
        }

        void receive_from(const asio::error_code &error, const size_t bytes_recvd) {
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }

            rbuffer.size = bytes_recvd;
            rbuffer.raw_data.resize(bytes_recvd);
            if (on_message_received)
                on_message_received(rbuffer, udp.remote_endpoint);

            consume_receive_buffer();
            udp.socket.async_receive_from(asio::buffer(rbuffer.raw_data, rbuffer.raw_data.size()),
                                          udp.remote_endpoint,
                                          std::bind(&UDPServer::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));
        }
    };
}
