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

#include <ranges>
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

        const std::set<std::shared_ptr<asio::ip::tcp::socket> > get_peers() const {
            return tcp.peers;
        }

        /*SETTINGS*/
        void set_max_send_buffer_size(int value = 1400) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*MESSAGE*/
        bool send_str_to(const std::string &message, std::shared_ptr<asio::ip::tcp::socket> &peer) {
            if (!get_acceptor().is_open() && !message.empty()) return false;

            asio::post(thread_pool, std::bind(&TCPServer::package_string, this, message, peer));
            return true;
        }

        bool send_buffer_to(const std::vector<std::byte> &buffer, std::shared_ptr<asio::ip::tcp::socket> &peer) {
            if (!get_acceptor().is_open() && !buffer.empty()) return false;

            asio::post(thread_pool, std::bind(&TCPServer::package_buffer, this, buffer, peer));
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
            if (!tcp.peers.empty())
                for (const std::shared_ptr<asio::ip::tcp::socket> &peer: tcp.peers) {
                    if (peer->is_open()) {
                        asio::error_code ec;
                        peer->shutdown(asio::ip::tcp::socket::shutdown_send, ec);
                        peer->close(ec);
                    }
                }
            tcp.context.stop();
            if (!tcp.peers.empty()) tcp.peers.clear();
            if (!response_buffers.empty()) response_buffers.clear();
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

        void disconnect_peer(const std::shared_ptr<asio::ip::tcp::socket> &peer) {
            asio::error_code ec;
            if (peer->is_open()) {
                peer->shutdown(asio::ip::tcp::socket::shutdown_send, ec);
                peer->close(ec);
                on_disconnected(peer, ec);
            }
            if (response_buffers.contains(peer))
                response_buffers.erase(peer);
            if (tcp.peers.contains(peer))
                tcp.peers.erase(peer);
        }

        /*ERRORS*/
        std::function<void(const std::shared_ptr<asio::ip::tcp::socket> &)> on_accept;
        std::function<void(const std::shared_ptr<asio::ip::tcp::socket> &, const asio::error_code &)> on_disconnected;
        std::function<void()> on_close;
        std::function<void(const size_t)> on_message_sent;
        std::function<void(const FTcpMessage, const std::shared_ptr<asio::ip::tcp::socket> &)> on_message_received;
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
        std::map<std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<asio::streambuf> > response_buffers;

        void package_string(const std::string &str, std::shared_ptr<asio::ip::tcp::socket> &peer) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || str.size() <= max_send_buffer_size) {
                asio::async_write(
                    *peer, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPServer::write, this, asio::placeholders::error,
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
                    *peer, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPServer::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                string_offset += package_size;
            }
        }

        void package_buffer(const std::vector<std::byte> &buffer, std::shared_ptr<asio::ip::tcp::socket> &peer) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || buffer.size() <= max_send_buffer_size) {
                asio::async_write(
                    *peer, asio::buffer(buffer.data(), buffer.size()),
                    std::bind(&TCPServer::write, this, asio::placeholders::error,
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
                    *peer, asio::buffer(sbuffer.data(), sbuffer.size()),
                    std::bind(&TCPServer::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                buffer_offset += package_size;
            }
        }

        void consume_response_buffer(const std::shared_ptr<asio::ip::tcp::socket> &peer) {
            if (response_buffers.contains(peer))
                return;
            const std::shared_ptr<asio::streambuf> streambuf = response_buffers[peer];
            const size_t size = streambuf->size();
            if (size > 0)
                streambuf->consume(size);
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            error_code.clear();
            std::shared_ptr<asio::ip::tcp::socket> conn_peer = std::make_shared<asio::ip::tcp::socket>(tcp.context);
            tcp.acceptor.async_accept(
                *conn_peer, std::bind(&TCPServer::accept, this, asio::placeholders::error, conn_peer));
            tcp.context.run();
            if (get_acceptor().is_open() && !is_closing)
                close();
        }

        void accept(const asio::error_code &error, std::shared_ptr<asio::ip::tcp::socket> &peer) {
            if (error) {
                error_code = error;
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error);
                return;
            }

            tcp.peers.insert(peer);
            std::shared_ptr<asio::streambuf> response_buffer = std::make_shared<asio::streambuf>();
            response_buffers.insert_or_assign(peer, response_buffer);
            asio::async_read(*peer, *response_buffers.at(peer), asio::transfer_at_least(1),
                             std::bind(&TCPServer::read, this, asio::placeholders::error,
                                       asio::placeholders::bytes_transferred, peer));

            if (on_accept) on_accept(peer);

            std::shared_ptr<asio::ip::tcp::socket> conn_peer = std::make_shared<asio::ip::tcp::socket>(tcp.context);
            tcp.acceptor.async_accept(
                *conn_peer, std::bind(&TCPServer::accept, this, asio::placeholders::error, conn_peer));
        }

        void write(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                error_code = error; {
                    std::lock_guard<std::mutex> guard(mutex_error);
                    if (on_error) on_error(error);
                }
                return;
            }
            if (on_message_sent) on_message_sent(bytes_sent);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd,
                  const std::shared_ptr<asio::ip::tcp::socket> &peer) {
            ;
            if (error) {
                error_code = error;
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error);
                if (peer->is_open() && !is_closing)
                    disconnect_peer(peer);
                return;
            }

            FTcpMessage rbuffer;
            rbuffer.size = bytes_recvd;
            rbuffer.raw_data.resize(bytes_recvd);
            asio::buffer_copy(asio::buffer(rbuffer.raw_data, bytes_recvd),
                              response_buffers.at(peer)->data());

            if (on_message_received) on_message_received(rbuffer, peer);

            consume_response_buffer(peer);
            asio::async_read(
                *peer, *response_buffers.at(peer), asio::transfer_at_least(1),
                std::bind(&TCPServer::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, peer));
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
        const std::set<std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > > &get_ssl_peers() {
            return tcp.ssl_peers;
        }

        /*SETTINGS*/
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
        bool send_str_to(const std::string &message,
                         const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            if (!get_acceptor().is_open() && !message.empty()) return false;

            asio::post(thread_pool, std::bind(&TCPServerSsl::package_string, this, message, ssl_peer));
            return true;
        }

        bool send_buffer_to(const std::vector<std::byte> &buffer,
                            const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            if (!get_acceptor().is_open() && !buffer.empty()) return false;

            asio::post(thread_pool, std::bind(&TCPServerSsl::package_buffer, this, buffer, ssl_peer));
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
            if (!tcp.ssl_peers.empty())
                for (const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer: tcp.ssl_peers) {
                    if (ssl_peer->next_layer().is_open()) {
                        asio::error_code ec;
                        ssl_peer->shutdown(ec);
                        ssl_peer->next_layer().close(ec);
                    }
                }
            tcp.context.stop();
            if (!tcp.ssl_peers.empty()) tcp.ssl_peers.clear();
            if (!response_buffers.empty()) response_buffers.clear();
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

        void disconnect_peer(const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            asio::error_code ec;
            if (ssl_peer->next_layer().is_open()) {
                ssl_peer->shutdown(ec);
                ssl_peer->next_layer().close(ec);
                on_disconnected(ssl_peer, ec);
            }
            if (response_buffers.contains(ssl_peer))
                response_buffers.erase(ssl_peer);
            if (tcp.ssl_peers.contains(ssl_peer))
                tcp.ssl_peers.erase(ssl_peer);
        }

        /*ERRORS*/
        std::function<void(const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &)> on_accept;
        std::function<void(const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &,
                           const asio::error_code &)> on_disconnected;
        std::function<void()> on_close;
        std::function<void(const size_t)> on_message_sent;
        std::function<void(const FTcpMessage, const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &)>
        on_message_received;
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
        std::map<std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> >, std::shared_ptr<asio::streambuf> >
        response_buffers;

        void package_string(const std::string &str,
                            const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || str.size() <= max_send_buffer_size) {
                asio::async_write(
                    *ssl_peer, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPServerSsl::write, this, asio::placeholders::error,
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
                    *ssl_peer, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPServerSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                string_offset += package_size;
            }
        }

        void package_buffer(const std::vector<std::byte> &buffer,
                            const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            std::lock_guard<std::mutex> guard(mutex_buffer);
            if (!split_buffer || buffer.size() <= max_send_buffer_size) {
                asio::async_write(
                    *ssl_peer, asio::buffer(buffer.data(), buffer.size()),
                    std::bind(&TCPServerSsl::write, this, asio::placeholders::error,
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
                    *ssl_peer, asio::buffer(sbuffer.data(), sbuffer.size()),
                    std::bind(&TCPServerSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                buffer_offset += package_size;
            }
        }

        void consume_response_buffer(const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &peer) {
            if (response_buffers.contains(peer))
                return;
            const std::shared_ptr<asio::streambuf> streambuf = response_buffers[peer];
            const size_t size = streambuf->size();
            if (size > 0)
                streambuf->consume(size);
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            error_code.clear();
            std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > conn_ssl_peer = std::make_shared<
                asio::ssl::stream<asio::ip::tcp::socket> >(tcp.context, tcp.ssl_context);
            tcp.acceptor.async_accept(conn_ssl_peer->lowest_layer(),
                                      std::bind(&TCPServerSsl::accept, this, asio::placeholders::error, conn_ssl_peer));
            tcp.context.run();
            if (get_acceptor().is_open() && !is_closing)
                close();
        }

        void accept(const asio::error_code &error,
                    std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            if (error) {
                error_code = error;
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error);
                return;
            }
            ssl_peer->async_handshake(asio::ssl::stream_base::server,
                                      std::bind(&TCPServerSsl::ssl_handshake, this,
                                                asio::placeholders::error, ssl_peer));
            std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > conn_peer = std::make_shared<asio::ssl::stream<
                asio::ip::tcp::socket> >(tcp.context, tcp.ssl_context);
            tcp.acceptor.async_accept(conn_peer->lowest_layer(),
                                      std::bind(&TCPServerSsl::accept, this, asio::placeholders::error, conn_peer));
        }

        void ssl_handshake(const asio::error_code &error,
                           std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            if (error) {
                error_code = error;
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error);
                return;
            }

            tcp.ssl_peers.insert(ssl_peer);
            std::shared_ptr<asio::streambuf> response_buffer = std::make_shared<asio::streambuf>();
            response_buffers.insert_or_assign(ssl_peer, response_buffer);
            asio::async_read(*ssl_peer, *response_buffers.at(ssl_peer), asio::transfer_at_least(1),
                             std::bind(&TCPServerSsl::read, this, asio::placeholders::error,
                                       asio::placeholders::bytes_transferred, ssl_peer));

            if (on_accept) on_accept(ssl_peer);
        }

        void write(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                error_code = error;
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error);
                return;
            }
            if (on_message_sent) on_message_sent(bytes_sent);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd,
                  const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            if (error) {
                error_code = error;
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error);
                if (ssl_peer->lowest_layer().is_open() && !is_closing)
                    disconnect_peer(ssl_peer);
                return;
            }

            FTcpMessage rbuffer;
            rbuffer.size = bytes_recvd;
            rbuffer.raw_data.resize(bytes_recvd);
            asio::buffer_copy(asio::buffer(rbuffer.raw_data, bytes_recvd),
                              response_buffers.at(ssl_peer)->data());

            if (on_message_received) on_message_received(rbuffer, ssl_peer);

            consume_response_buffer(ssl_peer);
            asio::async_read(
                *ssl_peer, *response_buffers.at(ssl_peer), asio::transfer_at_least(1),
                std::bind(&TCPServerSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, ssl_peer));
        }
    };
#endif
}
