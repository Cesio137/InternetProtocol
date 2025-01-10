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
    class TCPServer {
    public:
        ~TCPServer() {
            if (get_acceptor().is_open()) close();
        }

        /*SOCKET*/
        void set_socket(const Server::EServerProtocol protocol, const uint16_t port,
                        const int max_listen_conn = 2147483647) {
            tcp_protocol = protocol;
            tcp_port = port;
            max_listen_connection = max_listen_conn;
        }

        asio::ip::tcp::acceptor &get_acceptor() { return tcp.acceptor; }

        const std::set<socket_ptr > get_sockets() const {
            return tcp.sockets;
        }

        /*SETTINGS*/
        void set_max_send_buffer_size(int value = 1400) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*MESSAGE*/
        bool send_str_to(const std::string &message, const socket_ptr &socket) {
            if (!get_acceptor().is_open() || message.empty()) return false;

            asio::post(thread_pool, std::bind(&TCPServer::package_string, this, message, socket));
            return true;
        }

        bool send_buffer_to(const std::vector<std::byte> &buffer, const socket_ptr &socket) {
            if (!get_acceptor().is_open() || buffer.empty()) return false;

            asio::post(thread_pool, std::bind(&TCPServer::package_buffer, this, buffer, socket));
            return true;
        }

        /*CONNECTION*/
        bool open() {
            if (get_acceptor().is_open())
                return false;

            asio::ip::tcp::endpoint endpoint(tcp_protocol == Server::EServerProtocol::V4
                                                 ? asio::ip::tcp::v4()
                                                 : asio::ip::tcp::v6(), tcp_port);
            error_code = asio::error_code();
            tcp.acceptor.open(tcp_protocol == Server::EServerProtocol::V4
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
            tcp.acceptor.listen(max_listen_connection, error_code);
            if (error_code && on_error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }

            asio::post(thread_pool, std::bind(&TCPServer::run_context_thread, this));
            return true;
        }

        void close() {
            is_closing = true;
            if (!tcp.sockets.empty())
                for (const socket_ptr &socket: tcp.sockets) {
                    if (socket->is_open()) {
                        socket->shutdown(asio::ip::tcp::socket::shutdown_send, error_code);
                        if (on_socket_error && error_code) on_socket_error(error_code, socket);
                        socket->close(error_code);
                        if (on_socket_error && error_code) on_socket_error(error_code, socket);
                    }
                }
            tcp.context.stop();
            if (!tcp.sockets.empty()) tcp.sockets.clear();
            if (!listening_buffers.empty()) listening_buffers.clear();
            if (tcp.acceptor.is_open()) {
                tcp.acceptor.close(error_code); {
                    std::lock_guard<std::mutex> guard(mutex_error);
                    if (on_error) on_error(error_code);
                }
            }
            tcp.context.restart();
            if (on_close)
                on_close();
            is_closing = false;
        }

        void disconnect_socket(const socket_ptr &socket) {
            if (socket->is_open()) {
                socket->shutdown(asio::ip::tcp::socket::shutdown_send, error_code);
                if (on_socket_error && error_code) on_socket_error(error_code, socket);
                socket->close(error_code);
                if (on_socket_error && error_code) on_socket_error(error_code, socket);
            }
            if (listening_buffers.contains(socket))
                listening_buffers.erase(socket);
            if (tcp.sockets.contains(socket))
                tcp.sockets.erase(socket);
            if (on_socket_disconnected) on_socket_disconnected(socket);
        }

        /*ERRORS*/
        asio::error_code get_error_code() const { return error_code; }

        /*EVENTS*/
        std::function<void(const socket_ptr &)> on_socket_accepted;
        std::function<void(const size_t, const size_t)> on_bytes_transfered;
        std::function<void(const socket_ptr &)> on_message_sent;
        std::function<void(const FTcpMessage, const socket_ptr &)> on_message_received;
        std::function<void(const socket_ptr &)> on_socket_disconnected;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &, const socket_ptr &)> on_socket_error;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        std::mutex mutex_error;
        bool is_closing = false;
        Server::FAsioTcp tcp;
        asio::error_code error_code;
        Server::EServerProtocol tcp_protocol = Server::EServerProtocol::V4;
        uint16_t tcp_port = 3000;
        int max_listen_connection = 2147483647;
        bool split_buffer = true;
        int max_send_buffer_size = 1400;
        std::map<socket_ptr, std::shared_ptr<asio::streambuf> > listening_buffers;

        void package_string(const std::string &str, socket_ptr &socket) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || str.size() <= max_send_buffer_size) {
                asio::async_write(
                    *socket, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPServer::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, socket));
                return;
            }

            size_t string_offset = 0;
            const size_t max_size = max_send_buffer_size;
            while (string_offset < str.size()) {
                size_t package_size = std::min(max_size, str.size() - string_offset);
                std::string strshrink(str.begin() + string_offset,
                                      str.begin() + string_offset + package_size);
                asio::async_write(
                    *socket, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPServer::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, socket));
                string_offset += package_size;
            }
        }

        void package_buffer(const std::vector<std::byte> &buffer, socket_ptr &socket) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || buffer.size() <= max_send_buffer_size) {
                asio::async_write(
                    *socket, asio::buffer(buffer.data(), buffer.size()),
                    std::bind(&TCPServer::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, socket));
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
                    *socket, asio::buffer(sbuffer.data(), sbuffer.size()),
                    std::bind(&TCPServer::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, socket));
                buffer_offset += package_size;
            }
        }

        void consume_response_buffer(const socket_ptr &socket) {
            if (!listening_buffers.contains(socket))
                return;
            const std::shared_ptr<asio::streambuf> streambuf = listening_buffers[socket];
            const size_t size = streambuf->size();
            if (size > 0)
                streambuf->consume(size);
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            error_code.clear();
            socket_ptr conn_socket = std::make_shared<asio::ip::tcp::socket>(tcp.context);
            tcp.acceptor.async_accept(
                *conn_socket, std::bind(&TCPServer::accept, this, asio::placeholders::error, conn_socket));
            tcp.context.run();
            if (get_acceptor().is_open() && !is_closing)
                close();
        }

        void accept(const asio::error_code &error, socket_ptr &socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error, socket);
                return;
            }

            tcp.sockets.insert(socket);
            std::shared_ptr<asio::streambuf> response_buffer = std::make_shared<asio::streambuf>();
            listening_buffers.insert_or_assign(socket, response_buffer);
            asio::async_read(*socket, *listening_buffers.at(socket), asio::transfer_at_least(1),
                             std::bind(&TCPServer::read, this, asio::placeholders::error,
                                       asio::placeholders::bytes_transferred, socket));

            if (on_socket_accepted) on_socket_accepted(socket);

            socket_ptr conn_socket = std::make_shared<asio::ip::tcp::socket>(tcp.context);
            tcp.acceptor.async_accept(
                *conn_socket, std::bind(&TCPServer::accept, this, asio::placeholders::error, conn_socket));
        }

        void write(const asio::error_code &error, const size_t bytes_sent, const socket_ptr &socket) {
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error, socket);
                return;
            }
            if (on_message_sent) on_message_sent(socket);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd,
                  const socket_ptr &socket) {
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error, socket);
                if (socket->is_open() && !is_closing)
                    disconnect_socket(socket);
                return;
            }

            FTcpMessage rbuffer;
            rbuffer.size = bytes_recvd;
            rbuffer.raw_data.resize(bytes_recvd);
            asio::buffer_copy(asio::buffer(rbuffer.raw_data, bytes_recvd),
                              listening_buffers.at(socket)->data());

            if (on_message_received) on_message_received(rbuffer, socket);

            consume_response_buffer(socket);
            asio::async_read(
                *socket, *listening_buffers.at(socket), asio::transfer_at_least(1),
                std::bind(&TCPServer::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, socket));
        }
    };
#ifdef ASIO_USE_OPENSSL
    class TCPServerSsl {
    public:
        ~TCPServerSsl() {
            if (get_acceptor().is_open()) close();
        }

        /*SOCKET*/
        void set_socket(const Server::EServerProtocol protocol, const uint16_t port,
                        const int max_listen_conn = 2147483647) {
            tcp_protocol = protocol;
            tcp_port = port;
            max_listen_connection = max_listen_conn;
        }

        asio::ssl::context &get_ssl_context() { return tcp.ssl_context; }
        const asio::ip::tcp::acceptor &get_acceptor() { return tcp.acceptor; }
        const std::set<ssl_socket_ptr > &get_ssl_sockets() {
            return tcp.ssl_sockets;
        }

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
        bool send_str_to(const std::string &message,
                         const ssl_socket_ptr &ssl_socket) {
            if (!get_acceptor().is_open() || message.empty()) return false;

            asio::post(thread_pool, std::bind(&TCPServerSsl::package_string, this, message, ssl_socket));
            return true;
        }

        bool send_buffer_to(const std::vector<std::byte> &buffer,
                            const ssl_socket_ptr &ssl_socket) {
            if (!get_acceptor().is_open() || buffer.empty()) return false;

            asio::post(thread_pool, std::bind(&TCPServerSsl::package_buffer, this, buffer, ssl_socket));
            return true;
        }

        /*CONNECTION*/
        bool open() {
            if (get_acceptor().is_open())
                return false;

            asio::ip::tcp::endpoint endpoint(tcp_protocol == Server::EServerProtocol::V4
                                                 ? asio::ip::tcp::v4()
                                                 : asio::ip::tcp::v6(), tcp_port);
            tcp.acceptor.open(tcp_protocol == Server::EServerProtocol::V4
                                  ? asio::ip::tcp::v4()
                                  : asio::ip::tcp::v6(), error_code);
            if (error_code) {
                std::lock_guard<std::mutex> guard(mutex_error);
                on_error(error_code);
                return false;
            }
            tcp.acceptor.set_option(asio::socket_base::reuse_address(true), error_code);
            if (error_code) {
                std::lock_guard<std::mutex> guard(mutex_error);
                on_error(error_code);
                return false;
            }
            tcp.acceptor.bind(endpoint, error_code);
            if (error_code) {
                std::lock_guard<std::mutex> guard(mutex_error);
                on_error(error_code);
                return false;
            }
            tcp.acceptor.listen(max_listen_connection > 0
                                    ? max_listen_connection
                                    : asio::socket_base::max_listen_connections,
                                error_code);
            if (error_code) {
                std::lock_guard<std::mutex> guard(mutex_error);
                on_error(error_code);
                return false;
            }

            asio::post(thread_pool, std::bind(&TCPServerSsl::run_context_thread, this));
            return true;
        }

        void close() {
            is_closing = true;
            if (!tcp.ssl_sockets.empty())
                for (const ssl_socket_ptr &ssl_socket: tcp.ssl_sockets) {
                    if (ssl_socket->next_layer().is_open()) {
                        ssl_socket->shutdown(error_code);
                        if (on_socket_error && error_code) on_socket_error(error_code, ssl_socket);
                        ssl_socket->next_layer().close(error_code);
                        if (on_socket_error && error_code) on_socket_error(error_code, ssl_socket);
                    }
                }
            tcp.context.stop();
            if (!tcp.ssl_sockets.empty()) tcp.ssl_sockets.clear();
            if (!listening_buffers.empty()) listening_buffers.clear();
            if (tcp.acceptor.is_open()) {
                tcp.acceptor.close(error_code); {
                    std::lock_guard<std::mutex> guard(mutex_error);
                    if (on_error) on_error(error_code);
                }
            }
            tcp.context.restart();
            if (on_close) on_close();
            is_closing = false;
        }

        void disconnect_socket(const ssl_socket_ptr &ssl_socket) {
            if (ssl_socket->next_layer().is_open()) {
                ssl_socket->shutdown(error_code);
                ssl_socket->next_layer().close(error_code);
            }
            if (listening_buffers.contains(ssl_socket))
                listening_buffers.erase(ssl_socket);
            if (tcp.ssl_sockets.contains(ssl_socket))
                tcp.ssl_sockets.erase(ssl_socket);
            if (on_socket_disconnected) on_socket_disconnected(ssl_socket);
        }

        /*ERRORS*/
        asio::error_code get_error_code() const { return error_code; }

        /*EVENTS*/
        std::function<void(const ssl_socket_ptr &)> on_socket_accepted;
        std::function<void(const size_t, const size_t)> on_bytes_transfered;
        std::function<void(const ssl_socket_ptr &)> on_message_sent;
        std::function<void(const FTcpMessage, const ssl_socket_ptr &)> on_message_received;
        std::function<void(const ssl_socket_ptr &)> on_socket_disconnected;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &, const ssl_socket_ptr &)> on_socket_error;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        std::mutex mutex_error;
        bool is_closing = false;
        Server::FAsioTcpSsl tcp;
        asio::error_code error_code;
        Server::EServerProtocol tcp_protocol = Server::EServerProtocol::V4;
        uint16_t tcp_port = 3000;
        int max_listen_connection = 2147483647;
        bool split_buffer = true;
        int max_send_buffer_size = 1400;
        std::map<ssl_socket_ptr, std::shared_ptr<asio::streambuf> > listening_buffers;

        void package_string(const std::string &str,
                            const ssl_socket_ptr &ssl_socket) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || str.size() <= max_send_buffer_size) {
                asio::async_write(
                    *ssl_socket, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPServerSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, ssl_socket));
                return;
            }

            size_t string_offset = 0;
            const size_t max_size = max_send_buffer_size;
            while (string_offset < str.size()) {
                size_t package_size = std::min(max_size, str.size() - string_offset);
                std::string strshrink(str.begin() + string_offset,
                                      str.begin() + string_offset + package_size);
                asio::async_write(
                    *ssl_socket, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPServerSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, ssl_socket));
                string_offset += package_size;
            }
        }

        void package_buffer(const std::vector<std::byte> &buffer,
                            const ssl_socket_ptr &ssl_socket) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || buffer.size() <= max_send_buffer_size) {
                asio::async_write(
                    *ssl_socket, asio::buffer(buffer.data(), buffer.size()),
                    std::bind(&TCPServerSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, ssl_socket));
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
                    *ssl_socket, asio::buffer(sbuffer.data(), sbuffer.size()),
                    std::bind(&TCPServerSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, ssl_socket));
                buffer_offset += package_size;
            }
        }

        void consume_response_buffer(const ssl_socket_ptr &socket) {
            if (!listening_buffers.contains(socket))
                return;
            const std::shared_ptr<asio::streambuf> streambuf = listening_buffers[socket];
            const size_t size = streambuf->size();
            if (size > 0)
                streambuf->consume(size);
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            error_code.clear();
            ssl_socket_ptr conn_ssl_socket = std::make_shared<
                asio::ssl::stream<asio::ip::tcp::socket> >(tcp.context, tcp.ssl_context);
            tcp.acceptor.async_accept(conn_ssl_socket->lowest_layer(),
                                      std::bind(&TCPServerSsl::accept, this, asio::placeholders::error, conn_ssl_socket));
            tcp.context.run();
            if (get_acceptor().is_open() && !is_closing)
                close();
        }

        void accept(const asio::error_code &error,
                    ssl_socket_ptr &ssl_socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error, ssl_socket);
                return;
            }
            ssl_socket->async_handshake(asio::ssl::stream_base::server,
                                      std::bind(&TCPServerSsl::ssl_handshake, this,
                                                asio::placeholders::error, ssl_socket));
            ssl_socket_ptr conn_socket = std::make_shared<asio::ssl::stream<
                asio::ip::tcp::socket> >(tcp.context, tcp.ssl_context);
            tcp.acceptor.async_accept(conn_socket->lowest_layer(),
                                      std::bind(&TCPServerSsl::accept, this, asio::placeholders::error, conn_socket));
        }

        void ssl_handshake(const asio::error_code &error,
                           ssl_socket_ptr &ssl_socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error, ssl_socket);
                return;
            }

            tcp.ssl_sockets.insert(ssl_socket);
            std::shared_ptr<asio::streambuf> response_buffer = std::make_shared<asio::streambuf>();
            listening_buffers.insert_or_assign(ssl_socket, response_buffer);
            asio::async_read(*ssl_socket, *listening_buffers.at(ssl_socket), asio::transfer_at_least(1),
                             std::bind(&TCPServerSsl::read, this, asio::placeholders::error,
                                       asio::placeholders::bytes_transferred, ssl_socket));

            if (on_socket_accepted) on_socket_accepted(ssl_socket);
        }

        void write(const asio::error_code &error, const size_t bytes_sent, const ssl_socket_ptr &ssl_socket) {
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error, ssl_socket);
                return;
            }
            if (on_message_sent) on_message_sent(ssl_socket);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd,
                  const ssl_socket_ptr &ssl_socket) {
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_error) on_socket_error(error, ssl_socket);
                if (ssl_socket->lowest_layer().is_open() && !is_closing)
                    disconnect_socket(ssl_socket);
                return;
            }

            FTcpMessage rbuffer;
            rbuffer.size = bytes_recvd;
            rbuffer.raw_data.resize(bytes_recvd);
            asio::buffer_copy(asio::buffer(rbuffer.raw_data, bytes_recvd),
                              listening_buffers.at(ssl_socket)->data());

            if (on_message_received) on_message_received(rbuffer, ssl_socket);

            consume_response_buffer(ssl_socket);
            asio::async_read(
                *ssl_socket, *listening_buffers.at(ssl_socket), asio::transfer_at_least(1),
                std::bind(&TCPServerSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, ssl_socket));
        }
    };
#endif
}
