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
    class TCPClient {
    public:
        ~TCPClient() {
            should_stop_context = true;
            tcp.resolver.cancel();
            if (is_connected()) close();
            thread_pool->stop();
            consume_response_buffer();
        }

        /*HOST | LOCAL*/
        void set_host(const std::string &ip = "localhost", const std::string &port = "80") {
            host = ip;
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

        /*MESSAGE*/
        bool send(const std::string &message) {
            if (!thread_pool && !is_connected() && !message.empty()) return false;

            asio::post(*thread_pool, std::bind(&TCPClient::package_string, this, message));
            return true;
        }

        bool send_buffer(const std::vector<std::byte> &buffer) {
            if (!thread_pool && !is_connected() && !buffer.empty()) return false;;

            asio::post(*thread_pool, std::bind(&TCPClient::package_buffer, this, buffer));
            return true;
        }

        bool async_read() {
            if (!is_connected()) return false;

            asio::async_read(
                tcp.socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPClient::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
            return true;
        }

        /*CONNECTION*/
        bool connect() {
            if (!thread_pool && is_connected()) return false;

            asio::post(*thread_pool, std::bind(&TCPClient::run_context_thread, this));
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

        /*ERRORS*/
        asio::error_code get_error_code() const { return tcp.error_code; }

        /*EVENTS*/
        std::function<void()> on_connected;
        std::function<void()> on_close;
        std::function<void(const int)> on_connection_retry;
        std::function<void(const size_t)> on_message_sent;
        std::function<void(const FTcpMessage)> on_message_received;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::unique_ptr<asio::thread_pool> thread_pool =
                std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        bool should_stop_context = false;
        FAsioTcp tcp;
        std::string host = "localhost";
        std::string service;
        uint8_t timeout = 3;
        uint8_t max_attemp = 3;
        bool split_buffer = true;
        int max_send_buffer_size = 1400;
        asio::streambuf response_buffer;

        void package_string(const std::string &str) {
            mutex_buffer.lock();
            if (!split_buffer || str.size() <= max_send_buffer_size) {
                asio::async_write(
                    tcp.socket, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPClient::write, this, asio::placeholders::error,
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
                asio::async_write(
                    tcp.socket, asio::buffer(strshrink.data(), strshrink.size()),
                    std::bind(&TCPClient::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                string_offset += package_size;
            }
            mutex_buffer.unlock();
        }

        void package_buffer(const std::vector<std::byte> &buffer) {
            mutex_buffer.lock();
            if (!split_buffer || buffer.size() <= max_send_buffer_size) {
                asio::async_write(
                    tcp.socket, asio::buffer(buffer.data(), buffer.size()),
                    std::bind(&TCPClient::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                mutex_buffer.unlock();
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = max_send_buffer_size;
            while (buffer_offset < buffer.size()) {
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                std::vector<std::byte> sbuffer(
                    buffer.begin() + buffer_offset,
                    buffer.begin() + buffer_offset + package_size);
                asio::async_write(
                    tcp.socket, asio::buffer(sbuffer.data(), sbuffer.size()),
                    std::bind(&TCPClient::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                buffer_offset += package_size;
            }
            mutex_buffer.unlock();
        }

        void consume_response_buffer() {
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
                    std::bind(&TCPClient::resolve, this, asio::placeholders::error,
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
                if (on_error) on_error(tcp.error_code);
                return;
            }
            tcp.endpoints = endpoints;
            asio::async_connect(
                tcp.socket, tcp.endpoints,
                std::bind(&TCPClient::conn, this, asio::placeholders::error));
        }

        void conn(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }
            consume_response_buffer();
            asio::async_read(
                tcp.socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPClient::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));

            if (on_connected) on_connected();
        }

        void write(const asio::error_code &error, const size_t bytes_sent) {
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

            FTcpMessage rbuffer;
            rbuffer.size = bytes_recvd;
            rbuffer.raw_data.resize(bytes_recvd);
            asio::buffer_copy(asio::buffer(rbuffer.raw_data, bytes_recvd),
                              response_buffer.data());

            if (on_message_received) on_message_received(rbuffer);

            consume_response_buffer();
            asio::async_read(
                tcp.socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPClient::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
        }
    };
#ifdef ASIO_USE_OPENSSL
    class TCPClientSsl {
    public:
        ~TCPClientSsl() {
            should_stop_context = true;
            tcp.resolver.cancel();
            if (is_connected()) close();
            thread_pool->stop();
            consume_response_buffer();
        }

        /*HOST | LOCAL*/
        void set_host(const std::string &ip = "localhost", const std::string &port = "443") {
            host = ip;
            service = port;
        }

        std::string get_local_adress() const {
            return tcp.ssl_socket.lowest_layer()
                    .local_endpoint()
                    .address()
                    .to_string();
        }

        int get_local_port() const {
            return tcp.ssl_socket.lowest_layer().local_endpoint().port();
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

            asio::post(*thread_pool, std::bind(&TCPClientSsl::package_string, this, message));
            return true;
        }

        bool send_buffer(const std::vector<std::byte> &buffer) {
            if (!thread_pool && !is_connected() && !buffer.empty()) return false;;

            asio::post(*thread_pool, std::bind(&TCPClientSsl::package_buffer, this, buffer));
            return true;
        }

        bool async_read() {
            if (!is_connected()) return false;

            asio::async_read(
                tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPClientSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
            return true;
        }

        /*CONNECTION*/
        bool connect() {
            if (!thread_pool && is_connected()) return false;

            asio::post(*thread_pool, std::bind(&TCPClientSsl::run_context_thread, this));
            return true;
        }

        bool is_connected() const { return tcp.ssl_socket.lowest_layer().is_open(); }

        void close() {
            asio::error_code ec_shutdown;
            asio::error_code ec_close;
            tcp.context.stop();
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
        std::function<void()> on_close;
        std::function<void(const int)> on_connection_retry;
        std::function<void(const size_t)> on_message_sent;
        std::function<void(const FTcpMessage)> on_message_received;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::unique_ptr<asio::thread_pool> thread_pool =
                std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        bool should_stop_context = false;
        FAsioTcpSsl tcp;
        std::string host = "localhost";
        std::string service;
        uint8_t timeout = 3;
        uint8_t max_attemp = 3;
        bool split_buffer = true;
        int max_send_buffer_size = 1400;
        asio::streambuf response_buffer;

        void package_string(const std::string &str) {
            mutex_buffer.lock();
            if (!split_buffer || str.size() <= max_send_buffer_size) {
                asio::async_write(
                    tcp.ssl_socket, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPClientSsl::write, this, asio::placeholders::error,
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
                asio::async_write(
                    tcp.ssl_socket, asio::buffer(strshrink.data(), strshrink.size()),
                    std::bind(&TCPClientSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                string_offset += package_size;
            }
            mutex_buffer.unlock();
        }

        void package_buffer(const std::vector<std::byte> &buffer) {
            mutex_buffer.lock();
            if (!split_buffer || buffer.size() <= max_send_buffer_size) {
                asio::async_write(
                    tcp.ssl_socket, asio::buffer(buffer.data(), buffer.size()),
                    std::bind(&TCPClientSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                mutex_buffer.unlock();
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = max_send_buffer_size;
            while (buffer_offset < buffer.size()) {
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                std::vector<std::byte> sbuffer(
                    buffer.begin() + buffer_offset,
                    buffer.begin() + buffer_offset + package_size);
                asio::async_write(
                    tcp.ssl_socket, asio::buffer(sbuffer.data(), sbuffer.size()),
                    std::bind(&TCPClientSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                buffer_offset += package_size;
            }
            mutex_buffer.unlock();
        }

        void consume_response_buffer() {
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
                    std::bind(&TCPClientSsl::resolve, this, asio::placeholders::error,
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
                tcp.ssl_socket.lowest_layer(), tcp.endpoints,
                std::bind(&TCPClientSsl::conn, this, asio::placeholders::error));
        }

        void conn(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }
            tcp.ssl_socket.async_handshake(asio::ssl::stream_base::client,
                                           std::bind(&TCPClientSsl::ssl_handshake, this,
                                                     asio::placeholders::error));
        }

        void ssl_handshake(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }
            consume_response_buffer();
            asio::async_read(
                tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPClientSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));

            if (on_connected) on_connected();
        }

        void write(const asio::error_code &error, const size_t bytes_sent) {
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

            FTcpMessage rbuffer;
            rbuffer.size = bytes_recvd;
            rbuffer.raw_data.resize(bytes_recvd);
            asio::buffer_copy(asio::buffer(rbuffer.raw_data, bytes_recvd),
                              response_buffer.data());

            if (on_message_received) on_message_received(rbuffer);

            consume_response_buffer();
            asio::async_read(
                tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPClientSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
        }
    };
#endif
} // namespace InternetProtocol
