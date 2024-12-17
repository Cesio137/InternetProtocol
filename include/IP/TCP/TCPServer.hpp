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
    class TCPServer {
    public:
        ~TCPServer() {
            should_stop_context = true;
            if (get_acceptor().is_open()) close();
            thread_pool->stop();
            consume_response_buffer();
        }

        /*SOCKET*/
        bool set_socket(const Server::EServerProtocol protocol, const uint16_t port, const int max_connections,
                        asio::error_code &error_code) {
            if (tcp.acceptor.is_open())
                return false;

            asio::ip::tcp::endpoint endpoint(protocol == Server::EServerProtocol::V4
                                                 ? asio::ip::tcp::v4()
                                                 : asio::ip::tcp::v6(), port);
            tcp.acceptor.open(protocol == Server::EServerProtocol::V4
                                  ? asio::ip::tcp::v4()
                                  : asio::ip::tcp::v6(), error_code);
            if (error_code) return false;
            tcp.acceptor.set_option(asio::socket_base::reuse_address(true), error_code);
            if (error_code) return false;
            tcp.acceptor.bind(endpoint, error_code);
            if (error_code) return false;
            tcp.acceptor.listen(max_connections > 0 ? max_connections : asio::socket_base::max_listen_connections,
                                error_code);
            if (error_code) return false;
            return true;
        }

        const asio::ip::tcp::acceptor &get_acceptor() { return tcp.acceptor; }
        const std::set<std::shared_ptr<asio::ip::tcp::socket> > &get_peers() { return tcp.peers; }

        /*SETTINGS*/
        void set_max_send_buffer_size(int value = 1400) { max_send_buffer_size = value; }
        int get_max_send_buffer_size() const { return max_send_buffer_size; }
        void set_split_package(bool value = true) { split_buffer = value; }
        bool get_split_package() const { return split_buffer; }

        /*MESSAGE*/
        bool send_str_to(const std::string &message, const std::shared_ptr<asio::ip::tcp::socket> &peer) {
            if (!thread_pool && !get_acceptor().is_open() && !message.empty()) return false;

            asio::post(*thread_pool, std::bind(&TCPServer::package_string, this, message, peer));
            return true;
        }

        bool send_buffer_to(const std::vector<std::byte> &buffer, const std::shared_ptr<asio::ip::tcp::socket> &peer) {
            if (!thread_pool && !get_acceptor().is_open() && !buffer.empty()) return false;

            asio::post(*thread_pool, std::bind(&TCPServer::package_buffer, this, buffer, peer));
            return true;
        }

        /*CONNECTION*/
        bool open() {
            if (!thread_pool && !get_acceptor().is_open())
                return false;

            asio::post(*thread_pool, std::bind(&TCPServer::run_context_thread, this));
            return true;
        }

        void close() {
            tcp.context.stop();
            if (!tcp.peers.empty())
                for (const std::shared_ptr<asio::ip::tcp::socket> &peer: tcp.peers) {
                    if (peer->is_open()) {
                        peer->cancel();
                        peer->shutdown(asio::ip::tcp::socket::shutdown_both);
                        peer->close();
                    }
                }
            if (!tcp.acceptor.is_open()) return;
            tcp.acceptor.close();
            tcp.peers.clear();
            if (should_stop_context) return;
            if (on_close)
                on_close();
        }

        bool disconnect_peer(const std::shared_ptr<asio::ip::tcp::socket> &peer) {
            if (!peer || !peer->is_open()) {
                tcp.peers.erase(peer);
                return false;
            }
            peer->cancel();
            peer->shutdown(asio::ip::tcp::socket::shutdown_send);
            peer->close();
            return true;
        }

        /*ERRORS*/
        std::function<void(const std::shared_ptr<asio::ip::tcp::socket> &)> on_accept;
        std::function<void(const std::shared_ptr<asio::ip::tcp::socket> &)> on_disconnected;
        std::function<void()> on_close;
        std::function<void(const size_t)> on_message_sent;
        std::function<void(const FTcpMessage, const std::shared_ptr<asio::ip::tcp::socket> &)> on_message_received;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::unique_ptr<asio::thread_pool> thread_pool =
                std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        bool should_stop_context = false;
        Server::FAsioTcp tcp;
        bool split_buffer = true;
        int max_send_buffer_size = 1400;
        asio::streambuf response_buffer;

        void package_string(const std::string &str, const std::shared_ptr<asio::ip::tcp::socket> &peer) {
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

        void package_buffer(const std::vector<std::byte> &buffer, const std::shared_ptr<asio::ip::tcp::socket> &peer) {
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

        void consume_response_buffer() {
            response_buffer.consume(response_buffer.size());
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            tcp.error_code.clear();
            tcp.context.restart();
            std::shared_ptr<asio::ip::tcp::socket> conn_peer = std::make_shared<asio::ip::tcp::socket>(tcp.context);
            tcp.acceptor.async_accept(
                *conn_peer, std::bind(&TCPServer::accept, this, asio::placeholders::error, conn_peer));
            tcp.context.run();
        }

        void accept(const asio::error_code &error, std::shared_ptr<asio::ip::tcp::socket> &peer) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }

            if (on_accept) on_accept(peer);

            if (peer->is_open()) {
                tcp.peers.insert(peer);
                asio::async_read(*peer, response_buffer, asio::transfer_at_least(1),
                                 std::bind(&TCPServer::read, this, asio::placeholders::error,
                                           asio::placeholders::bytes_transferred, peer));
            }

            std::shared_ptr<asio::ip::tcp::socket> conn_peer = std::make_shared<asio::ip::tcp::socket>(tcp.context);
            tcp.acceptor.async_accept(
                *conn_peer, std::bind(&TCPServer::accept, this, asio::placeholders::error, conn_peer));
        }

        void write(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }
            if (on_message_sent) on_message_sent(bytes_sent);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd,
                  const std::shared_ptr<asio::ip::tcp::socket> &peer) {
            if (error) {
                if (error == asio::error::eof || error == asio::error::connection_reset || error.value() == 995) {
                    tcp.peers.erase(peer);
                    if (on_disconnected) on_disconnected(peer);
                } else {
                    tcp.error_code = error;
                    if (on_error) on_error(error);
                }
                return;
            }

            FTcpMessage rbuffer;
            rbuffer.size = bytes_recvd;
            rbuffer.raw_data.resize(bytes_recvd);
            asio::buffer_copy(asio::buffer(rbuffer.raw_data, bytes_recvd),
                              response_buffer.data());

            if (on_message_received) on_message_received(rbuffer, peer);

            consume_response_buffer();
            asio::async_read(
                *peer, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPServer::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, peer));
        }
    };
#ifdef ASIO_USE_OPENSSL
    class TCPServerSsl {
    public:
        ~TCPServerSsl() {
            should_stop_context = true;
            if (get_acceptor().is_open()) close();
            thread_pool->stop();
            consume_response_buffer();
        }

        /*SOCKET*/
        bool set_socket(const Server::EServerProtocol protocol, const uint16_t port, const int max_connections,
                        asio::error_code &error_code) {
            if (tcp.acceptor.is_open())
                return false;

            asio::ip::tcp::endpoint endpoint(protocol == Server::EServerProtocol::V4
                                                 ? asio::ip::tcp::v4()
                                                 : asio::ip::tcp::v6(), port);
            tcp.acceptor.open(protocol == Server::EServerProtocol::V4
                                  ? asio::ip::tcp::v4()
                                  : asio::ip::tcp::v6(), error_code);
            if (error_code) return false;
            tcp.acceptor.set_option(asio::socket_base::reuse_address(true), error_code);
            if (error_code) return false;
            tcp.acceptor.bind(endpoint, error_code);
            if (error_code) return false;
            tcp.acceptor.listen(max_connections > 0 ? max_connections : asio::socket_base::max_listen_connections,
                                error_code);
            if (error_code) return false;
            return true;
        }

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
            if (!thread_pool && !get_acceptor().is_open() && !message.empty()) return false;

            asio::post(*thread_pool, std::bind(&TCPServerSsl::package_string, this, message, ssl_peer));
            return true;
        }

        bool send_buffer_to(const std::vector<std::byte> &buffer,
                            const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            if (!thread_pool && !get_acceptor().is_open() && !buffer.empty()) return false;

            asio::post(*thread_pool, std::bind(&TCPServerSsl::package_buffer, this, buffer, ssl_peer));
            return true;
        }

        /*CONNECTION*/
        bool open() {
            if (!thread_pool && !get_acceptor().is_open())
                return false;

            asio::post(*thread_pool, std::bind(&TCPServerSsl::run_context_thread, this));
            return true;
        }

        void close() {
            tcp.context.stop();
            if (!tcp.ssl_peers.empty())
                for (const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>> &peer: tcp.ssl_peers) {
                    if (peer->lowest_layer().is_open()) {
                        peer->lowest_layer().cancel();
                        peer->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both);
                        peer->lowest_layer().close();
                    }
                }
            if (!tcp.acceptor.is_open()) return;
            tcp.acceptor.close();
            tcp.ssl_peers.clear();
            if (should_stop_context) return;
            if (on_close)
                on_close();
        }

        bool disconnect_peer(const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            if (!ssl_peer || !ssl_peer->lowest_layer().is_open()) {
                tcp.ssl_peers.erase(ssl_peer);
                return false;
            }
            ssl_peer->lowest_layer().cancel();
            ssl_peer->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_send);
            ssl_peer->lowest_layer().close();
            return true;
        }

        /*ERRORS*/
        std::function<void(const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &)> on_accept;
        std::function<void(const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &)> on_disconnected;
        std::function<void()> on_close;
        std::function<void(const size_t)> on_message_sent;
        std::function<void(const FTcpMessage, const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &)>
        on_message_received;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::unique_ptr<asio::thread_pool> thread_pool =
                std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutex_io;
        std::mutex mutex_buffer;
        bool should_stop_context = false;
        Server::FAsioTcpSsl tcp;
        bool split_buffer = true;
        int max_send_buffer_size = 1400;
        asio::streambuf response_buffer;

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

        void consume_response_buffer() {
            response_buffer.consume(response_buffer.size());
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            tcp.error_code.clear();
            tcp.context.restart();
            std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > conn_ssl_peer = std::make_shared<
                asio::ssl::stream<asio::ip::tcp::socket> >(tcp.context, tcp.ssl_context);
            tcp.acceptor.async_accept(conn_ssl_peer->lowest_layer(),
                                      std::bind(&TCPServerSsl::accept, this, asio::placeholders::error, conn_ssl_peer));
            tcp.context.run();
        }

        void accept(const asio::error_code &error,
                    std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            if (error) {
                tcp.error_code = error;
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
                           const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }
            if (on_accept) on_accept(ssl_peer);
            if (ssl_peer->lowest_layer().is_open()) {
                tcp.ssl_peers.insert(ssl_peer);
                asio::async_read(*ssl_peer, response_buffer, asio::transfer_at_least(1),
                                 std::bind(&TCPServerSsl::read, this, asio::placeholders::error,
                                           asio::placeholders::bytes_transferred, ssl_peer));
            }
        }

        void write(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                tcp.error_code = error;
                if (on_error) on_error(error);
                return;
            }
            if (on_message_sent) on_message_sent(bytes_sent);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd,
                  const std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > &ssl_peer) {
            if (error) {
                if (error == asio::error::eof || error == asio::error::connection_reset || error.value() == 995) {
                    tcp.ssl_peers.erase(ssl_peer);
                    if (on_disconnected) on_disconnected(ssl_peer);
                } else {
                    tcp.error_code = error;
                    if (on_error) on_error(error);
                }
                return;
            }

            FTcpMessage rbuffer;
            rbuffer.size = bytes_recvd;
            rbuffer.raw_data.resize(bytes_recvd);
            asio::buffer_copy(asio::buffer(rbuffer.raw_data, bytes_recvd),
                              response_buffer.data());

            if (on_message_received) on_message_received(rbuffer, ssl_peer);

            consume_response_buffer();
            asio::async_read(
                *ssl_peer, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPServerSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, ssl_peer));
        }
    };
#endif
}
