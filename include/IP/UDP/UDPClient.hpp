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
    class UDPClient {
    public:
        ~UDPClient() {
            should_stop_context = true;
            udp.resolver.cancel();
            if (is_connected()) close();
            thread_pool->stop();
            rbuffer.raw_data.clear();
            rbuffer.raw_data.shrink_to_fit();
        }

        /*HOST | LOCAL*/
        void set_host(const std::string &ip = "localhost", const std::string &port = "80") {
            host = ip;
            service = port;
        }

        std::string get_local_adress() const {
            return udp.socket.local_endpoint().address().to_string();
        }

        int get_local_port() const {
            return udp.socket.local_endpoint().port();
        }

        std::string get_remote_adress() const {
            return udp.socket.remote_endpoint().address().to_string();
        }

        int get_remote_port() const {
            return udp.socket.remote_endpoint().port();
        }

        /*SETTINGS*/
        void set_timeout(uint8_t value = 3) { timeout = value; }
        uint8_t get_timeout() const { return timeout; }
        void set_max_attemp(uint8_t value = 3) { max_attemp = value; }
        uint8_t get_max_attemp() const { return timeout; }
        void set_max_send_buffer_size(int value = 1024) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_max_receive_buffer_size(int value = 1024) { max_receive_buffer_size = value; }
        int get_max_receive_buffer_size() const { return max_receive_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*MESSAGE*/
        bool send(const std::string &message) {
            if (!thread_pool && !is_connected() && !message.empty())
                return false;

            asio::post(*thread_pool, std::bind(&UDPClient::package_string, this, message));
            return true;
        }

        bool send_raw(const std::vector<std::byte> &buffer) {
            if (!thread_pool && !is_connected() && !buffer.empty())
                return false;

            asio::post(*thread_pool, std::bind(&UDPClient::package_buffer, this, buffer));
            return true;
        }

        bool async_read() {
            if (!is_connected())
                return false;

            udp.socket.async_receive_from(asio::buffer(rbuffer.raw_data, rbuffer.raw_data.size()), udp.endpoints,
                                          std::bind(&UDPClient::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));
            return true;
        }

        /*CONNECTION*/
        bool connect() {
            if (!thread_pool && is_connected())
                return false;

            asio::post(*thread_pool, std::bind(&UDPClient::run_context_thread, this));
            return true;
        }

        bool is_connected() const { return udp.socket.is_open(); }

        void close() {
            asio::error_code ec_shutdown;
            asio::error_code ec_close;
            udp.context.stop();
            udp.socket.shutdown(asio::ip::udp::socket::shutdown_both, ec_shutdown);
            udp.socket.close(ec_close);
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

        /*ERRORS*/
        asio::error_code get_error_code() const { return udp.error_code; }

        /*EVENTS*/
        std::function<void()> on_connected;
        std::function<void()> on_close;
        std::function<void(int)> on_connection_retry;
        std::function<void(const size_t)> on_message_sent;
        std::function<void(const FUdpMessage)> on_message_received;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::unique_ptr<asio::thread_pool> thread_pool = std::make_unique<asio::thread_pool>(
            std::thread::hardware_concurrency());
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        bool should_stop_context = false;
        FAsioUdp udp;
        std::string host = "localhost";
        std::string service;
        uint8_t timeout = 3;
        uint8_t max_attemp = 3;
        bool split_buffer = true;
        int max_send_buffer_size = 1024;
        int max_receive_buffer_size = 1024;
        FUdpMessage rbuffer;

        void package_string(const std::string &str) {
            mutex_buffer.lock();
            if (!split_buffer || str.size() <= max_send_buffer_size) {
                udp.socket.async_send_to(asio::buffer(str.data(), str.size()), udp.endpoints,
                                         std::bind(&UDPClient::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                mutex_buffer.unlock();
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
            mutex_buffer.unlock();
        }

        void package_buffer(const std::vector<std::byte> &buffer) {
            mutex_buffer.lock();
            if (!split_buffer || buffer.size() <= max_send_buffer_size) {
                udp.socket.async_send_to(asio::buffer(buffer.data(), buffer.size()), udp.endpoints,
                                         std::bind(&UDPClient::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                mutex_buffer.unlock();
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = max_send_buffer_size;
            while (buffer_offset < buffer.size()) {
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                std::vector<std::byte> sbuffer(buffer.begin() + buffer_offset,
                                               buffer.begin() + buffer_offset + package_size);
                udp.socket.async_send_to(asio::buffer(sbuffer.data(), sbuffer.size()), udp.endpoints,
                                         std::bind(&UDPClient::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                buffer_offset += package_size;
            }
            mutex_buffer.unlock();
        }

        void consume_receive_buffer() {
            rbuffer.raw_data.clear();
            rbuffer.raw_data.shrink_to_fit();
            if (should_stop_context) return;
            rbuffer.raw_data.resize(max_receive_buffer_size);
        }

        /*ASYNC HANDLER FUNCTIONS*/
        void run_context_thread() {
            mutex_io.lock();
            while (udp.attemps_fail <= max_attemp) {
                if (on_connection_retry && udp.attemps_fail > 0)
                    on_connection_retry(udp.attemps_fail);
                udp.error_code.clear();
                udp.context.restart();
                udp.resolver.async_resolve(asio::ip::udp::v4(), host, service,
                                           std::bind(&UDPClient::resolve, this, asio::placeholders::error,
                                                     asio::placeholders::results));
                udp.context.run();
                if (!udp.error_code)
                    break;
                udp.attemps_fail++;
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
            }
            consume_receive_buffer();
            udp.attemps_fail = 0;
            mutex_io.unlock();
        }

        void resolve(const asio::error_code &error, const asio::ip::udp::resolver::results_type &results) {
            if (error) {
                udp.error_code = error;
                if (on_error)
                    on_error(error);
                return;
            }
            udp.endpoints = *results.begin();
            udp.socket.async_connect(udp.endpoints,
                                     std::bind(&UDPClient::conn, this, asio::placeholders::error));
        }

        void conn(const asio::error_code &error) {
            if (error) {
                udp.error_code = error;
                if (on_error)
                    on_error(error);
                return;
            }
            consume_receive_buffer();
            udp.socket.async_receive_from(asio::buffer(rbuffer.raw_data, rbuffer.raw_data.size()), udp.endpoints,
                                          std::bind(&UDPClient::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));

            if (on_connected)
                on_connected();
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
                return;
            }

            udp.attemps_fail = 0;
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
