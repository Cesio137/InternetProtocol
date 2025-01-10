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
            tcp.resolver.cancel();
            if (get_socket().is_open()) close();
            consume_response_buffer();
        }

        /*SOCKET*/
        void set_host(const std::string &ip = "localhost", const std::string &port = "3000") {
            host = ip;
            service = port;
        }

        asio::ip::tcp::socket &get_socket() { return tcp.socket; }

        /*SETTINGS*/
        void set_max_send_buffer_size(int value = 1400) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*MESSAGE*/
        bool send_str(const std::string &message) {
            if (!get_socket().is_open() || message.empty()) return false;

            asio::post(thread_pool, std::bind(&TCPClient::package_string, this, message));
            return true;
        }

        bool send_buffer(const std::vector<std::byte> &buffer) {
            if (!get_socket().is_open() || buffer.empty()) return false;;

            asio::post(thread_pool, std::bind(&TCPClient::package_buffer, this, buffer));
            return true;
        }

        /*CONNECTION*/
        bool connect() {
            if (get_socket().is_open())
                return false;

            asio::post(thread_pool, std::bind(&TCPClient::run_context_thread, this));
            return true;
        }

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

        /*ERRORS*/
        asio::error_code get_error_code() const { return error_code; }

        /*EVENTS*/
        std::function<void()> on_connected;
        std::function<void(const size_t, const size_t)> on_bytes_transfered;
        std::function<void()> on_message_sent;
        std::function<void(const FTcpMessage)> on_message_received;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &)> on_socket_error;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        std::mutex mutex_error;
        bool is_closing = false;
        Client::FAsioTcp tcp;
        asio::error_code error_code;
        std::string host = "localhost";
        std::string service = "3000";
        bool split_buffer = true;
        int max_send_buffer_size = 1400;
        asio::streambuf response_buffer;

        void package_string(const std::string &str) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || str.size() <= max_send_buffer_size) {
                asio::async_write(
                    tcp.socket, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPClient::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
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
        }

        void package_buffer(const std::vector<std::byte> &buffer) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || buffer.size() <= max_send_buffer_size) {
                asio::async_write(
                    tcp.socket, asio::buffer(buffer.data(), buffer.size()),
                    std::bind(&TCPClient::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
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
        }

        void consume_response_buffer() {
            const size_t size = response_buffer.size();
            if (size > 0)
                response_buffer.consume(size);
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            error_code.clear();
            tcp.resolver.async_resolve(
                host, service,
                std::bind(&TCPClient::resolve, this, asio::placeholders::error,
                          asio::placeholders::endpoint));
            tcp.context.run();
            if (get_socket().is_open() && !is_closing)
                close();
        }

        void resolve(const asio::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error_code);
                return;
            }
            tcp.endpoints = endpoints;
            asio::async_connect(
                tcp.socket, tcp.endpoints,
                std::bind(&TCPClient::conn, this, asio::placeholders::error));
        }

        void conn(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error_code);
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
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error_code);
                return;
            }
            if (on_message_sent) on_message_sent();
        }

        void read(const asio::error_code &error, const size_t bytes_recvd) {
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error_code);
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
            tcp.resolver.cancel();
            if (get_ssl_socket().lowest_layer().is_open()) close();
            consume_response_buffer();
        }

        /*HOST | LOCAL*/
        void set_host(const std::string &ip = "localhost", const std::string &port = "3000") {
            host = ip;
            service = port;
        }

        asio::ssl::context &get_ssl_context() { return tcp.ssl_context; }
        const asio::ssl::stream<asio::ip::tcp::socket> &get_ssl_socket() const { return tcp.ssl_socket; }
        void update_ssl_socket() { tcp.ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(tcp.context, tcp.ssl_context); }

        /*SETTINGS*/
        void set_max_send_buffer_size(int value = 1400) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

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
            if (!get_ssl_socket().lowest_layer().is_open() || message.empty()) return false;

            asio::post(thread_pool, std::bind(&TCPClientSsl::package_string, this, message));
            return true;
        }

        bool send_buffer(const std::vector<std::byte> &buffer) {
            if (!get_ssl_socket().lowest_layer().is_open() || buffer.empty()) return false;;

            asio::post(thread_pool, std::bind(&TCPClientSsl::package_buffer, this, buffer));
            return true;
        }

        /*CONNECTION*/
        bool connect() {
            if (get_ssl_socket().next_layer().is_open())
                return false;

            asio::post(thread_pool, std::bind(&TCPClientSsl::run_context_thread, this));
            return true;
        }

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
        std::function<void()> on_connected;
        std::function<void(const size_t, const size_t)> on_bytes_transfered;
        std::function<void()> on_message_sent;
        std::function<void(const FTcpMessage)> on_message_received;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &)> on_socket_error;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        std::mutex mutex_error;
        bool is_closing = false;
        Client::FAsioTcpSsl tcp;
        asio::error_code error_code;
        std::string host = "localhost";
        std::string service = "3000";
        bool split_buffer = true;
        int max_send_buffer_size = 1400;
        asio::streambuf response_buffer;

        void package_string(const std::string &str) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || str.size() <= max_send_buffer_size) {
                asio::async_write(
                    tcp.ssl_socket, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPClientSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
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
        }

        void package_buffer(const std::vector<std::byte> &buffer) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || buffer.size() <= max_send_buffer_size) {
                asio::async_write(
                    tcp.ssl_socket, asio::buffer(buffer.data(), buffer.size()),
                    std::bind(&TCPClientSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
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
        }

        void consume_response_buffer() {
            const size_t size = response_buffer.size();
            if (size > 0)
                response_buffer.consume(size);
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            error_code.clear();
            tcp.resolver.async_resolve(
                host, service,
                std::bind(&TCPClientSsl::resolve, this, asio::placeholders::error,
                          asio::placeholders::endpoint));
            tcp.context.run();
            if (get_ssl_socket().next_layer().is_open() && !is_closing)
                close();
        }

        void resolve(const asio::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            tcp.endpoints = endpoints;
            asio::async_connect(
                 tcp.ssl_socket.lowest_layer(), tcp.endpoints,
                std::bind(&TCPClientSsl::conn, this, asio::placeholders::error));
        }

        void conn(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }

             tcp.ssl_socket.async_handshake(asio::ssl::stream_base::client,
                                           std::bind(&TCPClientSsl::ssl_handshake, this,
                                                     asio::placeholders::error));
        }

        void ssl_handshake(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
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
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
                return;
            }
            if (on_message_sent) on_message_sent();
        }

        void read(const asio::error_code &error, const size_t bytes_recvd) {
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error);
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
