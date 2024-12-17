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

#include <iostream>
#include <IP/Net/Message.hpp>

namespace InternetProtocol {
    class UDPServer {
    public:
        ~UDPServer() {
            should_stop_context = true;
            if (udp.socket.is_open()) close();
            thread_pool->stop();
            rbuffer.raw_data.clear();
            rbuffer.raw_data.shrink_to_fit();
        }

        /*HOST | LOCAL*/
        bool set_socket(const Server::EServerProtocol protocol, const uint16_t port, asio::error_code &error_code) {
            if (udp.socket.is_open())
                return false;

            udp.socket.open(protocol == Server::EServerProtocol::V4 ? asio::ip::udp::v4() : asio::ip::udp::v6(),
                            error_code);
            if (error_code) return false;
            udp.socket.bind(
                asio::ip::udp::endpoint(protocol == Server::EServerProtocol::V4
                                            ? asio::ip::udp::v4()
                                            : asio::ip::udp::v6(), port), error_code);
            if (error_code) return false;
            return true;
        }

        const asio::ip::udp::socket &get_socket() const { return udp.socket; }

        /*SETTINGS*/
        void set_max_send_buffer_size(int value = 1024) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_max_receive_buffer_size(int value = 1024) { max_receive_buffer_size = value; }
        int get_max_receive_buffer_size() const { return max_receive_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*MESSAGE*/
        bool send_str_to(const std::string &message, const asio::ip::udp::endpoint &endpoint) {
            if (!thread_pool && !get_socket().is_open() && !message.empty())
                return false;

            asio::post(*thread_pool, std::bind(&UDPServer::package_string, this, message, endpoint));
            return true;
        }

        bool send_buffer_to(const std::vector<std::byte> &buffer, const asio::ip::udp::endpoint &endpoint) {
            if (!thread_pool && !get_socket().is_open() && !buffer.empty())
                return false;

            asio::post(*thread_pool, std::bind(&UDPServer::package_buffer, this, buffer, endpoint));
            return true;
        }

        bool async_read() {
            if (!get_socket().is_open())
                return false;

            udp.socket.async_receive_from(asio::buffer(rbuffer.raw_data, rbuffer.raw_data.size()), udp.remote_endpoint,
                                          std::bind(&UDPServer::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));
            return true;
        }

        /*CONNECTION*/
        bool open() {
            if (!udp.socket.is_open())
                return false;

            asio::post(*thread_pool, std::bind(&UDPServer::run_context_thread, this));
            return true;
        }

        void close() {
            udp.context.stop();
            if (!udp.socket.is_open()) return;
            udp.socket.cancel();
            udp.socket.shutdown(asio::ip::udp::socket::shutdown_both);
            udp.socket.close();
            if (should_stop_context) return;
            if (on_close)
                on_close();
        }

        /*ERRORS*/
        std::function<void()> on_close;
        std::function<void(const size_t)> on_message_sent;
        std::function<void(const FUdpMessage, const asio::ip::udp::endpoint &)> on_message_received;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::unique_ptr<asio::thread_pool> thread_pool = std::make_unique<asio::thread_pool>(
            std::thread::hardware_concurrency());
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        Server::FAsioUdp udp;
        bool should_stop_context = false;
        bool split_buffer = true;
        int max_send_buffer_size = 1024;
        int max_receive_buffer_size = 1024;
        FUdpMessage rbuffer;

        void package_string(const std::string &str, const asio::ip::udp::endpoint &endpoint) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || str.size() <= max_send_buffer_size) {
                udp.socket.async_send_to(asio::buffer(str.data(), str.size()), endpoint,
                                         std::bind(&UDPServer::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
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
                                                   asio::placeholders::bytes_transferred));
                string_offset += package_size;
            }
        }

        void package_buffer(const std::vector<std::byte> &buffer, const asio::ip::udp::endpoint &endpoint) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || buffer.size() <= max_send_buffer_size) {
                udp.socket.async_send_to(asio::buffer(buffer.data(), buffer.size()), endpoint,
                                         std::bind(&UDPServer::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = max_send_buffer_size;
            while (buffer_offset < buffer.size()) {
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                std::vector<std::byte> sbuffer(buffer.begin() + buffer_offset,
                                               buffer.begin() + buffer_offset + package_size);
                udp.socket.async_send_to(asio::buffer(sbuffer.data(), sbuffer.size()), endpoint,
                                         std::bind(&UDPServer::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                buffer_offset += package_size;
            }
        }

        void consume_receive_buffer() {
            rbuffer.raw_data.clear();
            rbuffer.raw_data.shrink_to_fit();
            rbuffer.raw_data.resize(max_receive_buffer_size);
        }

        /*ASYNC HANDLER FUNCTIONS*/
        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            udp.error_code.clear();
            udp.context.restart();
            udp.socket.async_receive_from(asio::buffer(rbuffer.raw_data, rbuffer.raw_data.size()),
                                          udp.remote_endpoint,
                                          std::bind(&UDPServer::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));
            udp.context.run();
        }

        void send_to(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                if (on_error)
                    on_error(error);
                return;
            }

            if (on_message_sent)
                on_message_sent(bytes_sent);
        }

        void receive_from(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                if (on_error)
                    on_error(error);
                udp.socket.async_receive_from(asio::buffer(rbuffer.raw_data, rbuffer.raw_data.size()),
                                              udp.remote_endpoint,
                                              std::bind(&UDPServer::receive_from, this, asio::placeholders::error,
                                                        asio::placeholders::bytes_transferred));
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
