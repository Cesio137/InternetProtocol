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
#include "IP/Net/Utils.hpp"

namespace InternetProtocol {
    class HttpServer {
    public:
        HttpServer() {
            headers.insert_or_assign("Server", "ASIO");
            headers.insert_or_assign("Connection", "close");
        }

        ~HttpServer() {
            if (get_acceptor().is_open()) close();
        }

        /* SOCKET */
        void set_socket(const EProtocolType protocol, const uint16_t port, const int max_listen_conn = 2147483647) {
            tcp_protocol = protocol;
            tcp_port = port;
            backlog = max_listen_conn;
        }

        asio::ip::tcp::acceptor &get_acceptor() { return tcp.acceptor; }

        const std::set<socket_ptr> get_sockets() const {
            return tcp.sockets;
        }

        /* HEADERS */
        void append_headers(const std::string &key, const std::string &value) {
            headers[key] = value;
        }

        void clear_headers() { headers.clear(); }
        void remove_header(const std::string &key) { headers.erase(key); }

        bool has_header(const std::string &key) const {
            return headers.contains(key);
        }

        /* RESPONSE */
        bool send_response(Server::FResponse &response, const socket_ptr &socket) {
            if (!socket->is_open()) return false;
            asio::post(thread_pool, std::bind(&HttpServer::process_response, this, response, socket));
            return true;
        }

        bool send_error_response(const uint32_t status_code, const Server::FResponse &response,
                                 const socket_ptr &socket) {
            if (!socket->is_open()) return false;
            asio::post(thread_pool,
                       std::bind(&HttpServer::process_error_response, this, status_code, response, socket));
            return true;
        }

        /* CONNECTION */
        bool open() {
            if (get_acceptor().is_open()) return false;

            asio::ip::tcp::endpoint endpoint(
                tcp_protocol == EProtocolType::V4 ? asio::ip::tcp::v4() : asio::ip::tcp::v6(), tcp_port);
            tcp.acceptor.open(tcp_protocol == EProtocolType::V4 ? asio::ip::tcp::v4() : asio::ip::tcp::v6(),
                              error_code);
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
            tcp.acceptor.listen(backlog, error_code);
            if (error_code) {
                std::lock_guard<std::mutex> guard(mutex_error);
                on_error(error_code);
                return false;
            }

            asio::post(thread_pool, std::bind(&HttpServer::run_context_thread, this));
            return true;
        }

        void close() {
            is_closing = true;
            if (!tcp.sockets.empty()) {
                for (const socket_ptr &socket: tcp.sockets) {
                    if (socket->is_open()) {
                        std::lock_guard<std::mutex> guard(mutex_error);
                        bool has_error = false;
                        socket->shutdown(asio::ip::tcp::socket::shutdown_send, error_code);
                        if (on_socket_disconnected && error_code) {
                            has_error = true;
                            on_socket_disconnected(error_code, socket);
                        }
                        socket->close(error_code);
                        if (on_socket_disconnected && error_code) {
                            has_error = true;
                            on_socket_disconnected(error_code, socket);
                        }
                        if (on_socket_disconnected && !has_error) {
                            on_socket_disconnected(asio::error_code(), socket);
                        }
                    }
                }
            }
            tcp.context.stop();
            if (!request_buffers.empty()) request_buffers.clear();
            if (!tcp.sockets.empty()) tcp.sockets.clear();
            if (tcp.acceptor.is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                tcp.acceptor.close(error_code);
                if (on_error) on_error(error_code);
            }
            tcp.context.restart();
            tcp.acceptor = asio::ip::tcp::acceptor(tcp.context);
            if (on_close) on_close();
            is_closing = false;
        }

        void disconnect_socket(const socket_ptr &socket) {
            bool has_error = false;
            if (socket->is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                socket->shutdown(asio::ip::tcp::socket::shutdown_send, error_code);
                if (on_socket_disconnected && error_code) {
                    has_error = true;
                    on_socket_disconnected(error_code, socket);
                }
                socket->close(error_code);
                if (on_socket_disconnected && error_code) {
                    has_error = true;
                    on_socket_disconnected(error_code, socket);
                }
            }
            if (request_buffers.contains(socket)) request_buffers.erase(socket);
            if (tcp.sockets.contains(socket)) tcp.sockets.erase(socket);
            if (on_socket_disconnected && !has_error) on_socket_disconnected(asio::error_code(), socket);
        }

        /* ERRORS */
        asio::error_code get_error_code() const { return error_code; }

        /* EVENTS */
        std::function<void(const socket_ptr &)> on_socket_accepted;
        std::function<void(const size_t, const size_t)> on_bytes_transfered;
        std::function<void(const Server::FRequest, Server::FResponse &, const socket_ptr &)> on_request_received;
        std::function<void(const asio::error_code &, const socket_ptr &)> on_request_error;
        std::function<void(const asio::error_code &, const socket_ptr &)> on_response_sent;
        std::function<void(const asio::error_code &, const socket_ptr &)> on_socket_disconnected;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_error;
        bool is_closing = false;
        Server::FAsioTcp tcp;
        asio::error_code error_code;
        EProtocolType tcp_protocol = EProtocolType::V4;
        uint16_t tcp_port = 3000;
        int backlog = 2147483647;
        std::map<std::string, std::string> headers;
        std::map<socket_ptr, std::shared_ptr<asio::streambuf> > request_buffers;

        void disconnect_socket_after_error(const asio::error_code &error, const socket_ptr &socket) {
            if (socket->is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                socket->shutdown(asio::ip::tcp::socket::shutdown_send, error_code);
                if (on_socket_disconnected && error_code) {
                    on_socket_disconnected(error_code, socket);
                }
                socket->close(error_code);
                if (on_socket_disconnected && error_code) {
                    on_socket_disconnected(error_code, socket);
                }
            }
            if (request_buffers.contains(socket)) request_buffers.erase(socket);
            if (tcp.sockets.contains(socket)) tcp.sockets.erase(socket);
            if (on_socket_disconnected) on_socket_disconnected(error, socket);
        }

        void process_response(const Server::FResponse &response, const socket_ptr &socket) {
            std::string payload = "HTTP/" + response.version + " 200 OK\r\n";
            if (!response.headers.empty()) {
                for (const std::pair<std::string, std::string> header: response.headers) {
                    payload += header.first + ": " + header.second + "\r\n";
                }
                payload += "Content-Length: " + std::to_string(response.body.length()) + "\r\n";
            }
            payload += "\r\n";
            if (!response.body.empty()) payload += response.body + "\r\n";

            const bool should_close = response.headers.contains("Connection")
                                          ? (response.headers.at("Connection") == "close")
                                          : true;

            asio::async_write(
                *socket, asio::buffer(payload.data(), payload.size()),
                std::bind(&HttpServer::write_response, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, socket, should_close));
        }

        void process_error_response(const uint32_t status_code, const Server::FResponse &response,
                                    const socket_ptr &socket) {
            std::string payload = "HTTP/1.1 " + std::to_string(status_code) + " " + ResponseStatusCode.at(status_code) +
                                  "\r\n";
            if (response.headers.size() > 0) {
                for (const std::pair<std::string, std::string> header: response.headers) {
                    payload += header.first + ": " + header.second + "\r\n";
                }
                if (!response.headers.contains("Content-Length")) {
                    payload += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
                }
            }
            payload += "\r\n";
            if (response.body.size() > 0) {
                payload += response.body + "\r\n";
            }
            asio::async_write(
                *socket, asio::buffer(payload.data(), payload.size()),
                std::bind(&HttpServer::write_response, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, socket, true));
        }

        void consume_request_buffer(const socket_ptr &socket) {
            if (!request_buffers.contains(socket)) return;
            const std::shared_ptr<asio::streambuf> streambuf = request_buffers[socket];
            const size_t size = streambuf->size();
            if (size > 0) streambuf->consume(size);
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            error_code.clear();
            socket_ptr conn_socket = std::make_shared<asio::ip::tcp::socket>(tcp.context);
            tcp.acceptor.async_accept(
                *conn_socket, std::bind(&HttpServer::accept, this, asio::placeholders::error, conn_socket));
            tcp.context.run();
            if (!is_closing) close();
        }

        void accept(const asio::error_code &error, socket_ptr &socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_socket_disconnected) on_socket_disconnected(error, socket);
                if (tcp.acceptor.is_open()) {
                    socket_ptr conn_socket = std::make_shared<asio::ip::tcp::socket>(tcp.context);
                    tcp.acceptor.async_accept(
                        *conn_socket, std::bind(&HttpServer::accept, this, asio::placeholders::error, conn_socket));
                }
                return;
            }

            if (tcp.sockets.size() < backlog) {
                tcp.sockets.insert(socket);
                std::shared_ptr<asio::streambuf> request_buffer = std::make_shared<asio::streambuf>();
                request_buffers.insert_or_assign(socket, request_buffer);
                asio::async_read_until(*socket, *request_buffer, "\r\n",
                                       std::bind(&HttpServer::read_status_line, this,
                                                 asio::placeholders::error, asio::placeholders::bytes_transferred,
                                                 socket));

                if (on_socket_accepted) on_socket_accepted(socket);
            } else {
                std::lock_guard<std::mutex> guard(mutex_error);
                disconnect_socket_after_error(error, socket);
            }
            if (tcp.acceptor.is_open()) {
                socket_ptr conn_socket = std::make_shared<asio::ip::tcp::socket>(tcp.context);
                tcp.acceptor.async_accept(
                    *conn_socket, std::bind(&HttpServer::accept, this, asio::placeholders::error, conn_socket));
            }
        }

        void write_response(const asio::error_code &error, const size_t bytes_sent, const socket_ptr &socket,
                            const bool close_connection) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_response_sent) on_response_sent(error, socket);
                if (!is_closing) disconnect_socket_after_error(error, socket);
                return;
            }

            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (close_connection) disconnect_socket(socket);
            else {
                asio::async_read_until(*socket, *request_buffers.at(socket), "\r\n",
                                       std::bind(&HttpServer::read_status_line, this,
                                                 asio::placeholders::error, asio::placeholders::bytes_transferred,
                                                 socket));
            }

            if (on_response_sent) on_response_sent(error, socket);
        }

        void read_status_line(const asio::error_code &error, const size_t bytes_recvd, const socket_ptr &socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (!is_closing) disconnect_socket_after_error(error, socket);
                return;
            }

            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            std::istream response_stream(request_buffers.at(socket).get());
            std::string method;
            response_stream >> method;
            std::string path;
            response_stream >> path;
            std::string version;
            response_stream >> version;

            std::string error_payload;
            std::set<std::string> versions = {"HTTP/1.0", "HTTP/1.1", "HTTP/2.0"};
            if (!Server::RequestMethod.contains(method)) {
                Server::FResponse response;
                if (!versions.contains(version)) {
                    version = "1.1";
                } else {
                    version.erase(0, 5);
                }
                response.version = version;
                response.headers.insert_or_assign("Allow", "DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT, TRACE");
                response.headers.insert_or_assign("Content-Type", "text/plain");
                response.headers.insert_or_assign("Connection", "close");
                response.body = "Method \"" + method + "\" not allowed";
                process_error_response(405, response, socket);
                return;
            }
            if (!versions.contains(version)) {
                Server::FResponse response;
                response.version = "1.1";
                response.headers.insert_or_assign("Content-Type", "text/plain");
                response.headers.insert_or_assign("Connection", "close");
                response.body = "The server does not support the HTTP version used in the request.";
                process_error_response(505, response, socket);
                return;
            }

            Server::FRequest request;
            version.erase(0, 5);
            request.version = version;
            request.method = Server::RequestMethod.at(method);
            request.path = path;

            request_buffers.at(socket)->consume(2);
            asio::async_read_until(
                *socket, *request_buffers.at(socket), "\r\n\r\n",
                std::bind(&HttpServer::read_headers, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, request, socket));
        }

        void read_headers(const asio::error_code &error, const size_t bytes_recvd, Server::FRequest &request,
                          const socket_ptr &socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (!is_closing) disconnect_socket_after_error(error, socket);
                return;
            }
            std::istream response_stream(request_buffers.at(socket).get());
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                Server::req_append_header(request, header);

            read_body(request, socket);
        }

        void read_body(Server::FRequest &request, const socket_ptr &socket) {
            if (request_buffers.at(socket)->size() == 0) {
                if (on_request_received) {
                    Server::FResponse response;
                    response.version = request.version;
                    response.headers = headers;
                    if (request.headers.contains("Connection"))
                        response.headers["Connection"] = request.headers["Connection"][0];
                    on_request_received(request, response, socket);
                    consume_request_buffer(socket);
                }
                return;
            }

            std::ostringstream body_buffer;
            body_buffer << request_buffers.at(socket);
            Server::req_set_body(request, body_buffer.str());

            if (on_request_received) {
                Server::FResponse response;
                response.version = request.version;
                response.headers = headers;
                if (request.headers.contains("Connection"))
                    response.headers["Connection"] = request.headers["Connection"][0];
                on_request_received(request, response, socket);
            }
            consume_request_buffer(socket);
        }
    };
#ifdef ASIO_USE_OPENSSL
    class HttpServerSsl {
    public:
        HttpServerSsl() {
            headers.insert_or_assign("Server", "ASIO");
            headers.insert_or_assign("Connection", "close");
        }

        ~HttpServerSsl() {
            if (get_acceptor().is_open()) close();
        }

        /*SOCKET*/
        void set_socket(const EProtocolType protocol, const uint16_t port,
                        const int max_listen_conn = 2147483647) {
            tcp_protocol = protocol;
            tcp_port = port;
            backlog = max_listen_conn;
        }

        asio::ssl::context &get_ssl_context() { return tcp.ssl_context; }
        asio::ip::tcp::acceptor &get_acceptor() { return tcp.acceptor; }

        const std::set<ssl_socket_ptr> get_sockets() const {
            return tcp.ssl_sockets;
        }

        /*HEADERS*/
        void append_headers(const std::string &key, const std::string &value) {
            headers[key] = value;
        }

        void clear_headers() { headers.clear(); }
        void remove_header(const std::string &key) { headers.erase(key); }

        bool has_header(const std::string &key) const {
            return headers.contains(key);
        }

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

        /*RESPONSE*/
        bool send_response(Server::FResponse &response, const ssl_socket_ptr &ssl_socket) {
            if (!ssl_socket->next_layer().is_open()) return false;
            asio::post(thread_pool, std::bind(&HttpServerSsl::process_response, this, response, ssl_socket));
            return true;
        }

        bool send_error_response(const uint32_t status_code, const Server::FResponse &response,
                                 const ssl_socket_ptr &ssl_socket) {
            if (!ssl_socket->next_layer().is_open()) return false;
            asio::post(thread_pool,
                       std::bind(&HttpServerSsl::process_error_response, this, status_code, response, ssl_socket));
            return true;
        }

        /*CONNECTION*/
        bool open() {
            if (get_acceptor().is_open())
                return false;

            asio::ip::tcp::endpoint endpoint(tcp_protocol == EProtocolType::V4
                                                 ? asio::ip::tcp::v4()
                                                 : asio::ip::tcp::v6(), tcp_port);
            error_code = asio::error_code();
            tcp.acceptor.open(tcp_protocol == EProtocolType::V4
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
            tcp.acceptor.listen(backlog, error_code);
            if (error_code && on_error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }

            asio::post(thread_pool, std::bind(&HttpServerSsl::run_context_thread, this));
            return true;
        }

        void close() {
            is_closing = true;
            if (!tcp.ssl_sockets.empty())
                for (const ssl_socket_ptr &ssl_socket: tcp.ssl_sockets) {
                    if (ssl_socket->next_layer().is_open()) {
                        std::lock_guard<std::mutex> guard(mutex_error);
                        ssl_socket->shutdown(error_code);
                        bool has_error = false;
                        if (on_socket_disconnected && error_code) {
                            has_error = true;
                            on_socket_disconnected(error_code, ssl_socket);
                        }
                        ssl_socket->next_layer().close(error_code);
                        if (on_socket_disconnected && error_code) {
                            has_error = true;
                            on_socket_disconnected(error_code, ssl_socket);
                        }
                        if (on_socket_disconnected && !has_error)
                            on_socket_disconnected(asio::error_code(), ssl_socket);
                    }
                }
            tcp.context.stop();
            if (!tcp.ssl_sockets.empty()) tcp.ssl_sockets.clear();
            if (!request_buffers.empty()) request_buffers.clear();
            if (tcp.acceptor.is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                tcp.acceptor.close(error_code);
                if (on_error) on_error(error_code);
            }
            tcp.context.restart();
            tcp.acceptor = asio::ip::tcp::acceptor(tcp.context);
            if (on_close) on_close();
            is_closing = false;
        }

        void disconnect_socket(const ssl_socket_ptr &ssl_socket) {
            bool has_error = false;
            if (ssl_socket->next_layer().is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                ssl_socket->shutdown(error_code);
                if (on_socket_disconnected && error_code) {
                    has_error = true;
                    on_socket_disconnected(error_code, ssl_socket);
                }
                ssl_socket->next_layer().close(error_code);
                if (on_socket_disconnected && error_code) {
                    has_error = true;
                    on_socket_disconnected(error_code, ssl_socket);
                }
            }
            if (request_buffers.contains(ssl_socket))
                request_buffers.erase(ssl_socket);
            if (tcp.ssl_sockets.contains(ssl_socket))
                tcp.ssl_sockets.erase(ssl_socket);
            if (on_socket_disconnected, !has_error) on_socket_disconnected(asio::error_code(), ssl_socket);
        }

        /*ERRORS*/
        asio::error_code get_error_code() const { return error_code; }

        /*EVENTS*/
        std::function<void(const ssl_socket_ptr &)> on_socket_accepted;
        std::function<void(const size_t, const size_t)> on_bytes_transfered;
        std::function<void(const Server::FRequest, Server::FResponse, const ssl_socket_ptr &)> on_request_received;
        std::function<void(const asio::error_code &, const ssl_socket_ptr &)> on_request_error;
        std::function<void(const asio::error_code &, const ssl_socket_ptr &)> on_response_sent;
        std::function<void(const asio::error_code &, const ssl_socket_ptr &)> on_socket_disconnected;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_error;
        bool is_closing = false;
        Server::FAsioTcpSsl tcp;
        asio::error_code error_code;
        EProtocolType tcp_protocol = EProtocolType::V4;
        uint16_t tcp_port = 3000;
        std::map<std::string, std::string> headers;
        int backlog = 2147483647;
        std::map<std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> >, std::shared_ptr<asio::streambuf> >
        request_buffers;
        const std::map<std::string, EMethod> verb = {
            {"DELETE", EMethod::DEL}, {"GET", EMethod::GET},
            {"HEAD", EMethod::HEAD}, {"OPTIONS", EMethod::OPTIONS},
            {"PATCH", EMethod::PATCH}, {"POST", EMethod::POST},
            {"PUT", EMethod::PUT}, {"TRACE", EMethod::TRACE},
        };

        void disconnect_socket_after_error(const asio::error_code &error, const ssl_socket_ptr &ssl_socket) {
            bool has_error = false;
            if (ssl_socket->next_layer().is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                ssl_socket->shutdown(error_code);
                if (on_socket_disconnected && error_code) {
                    on_socket_disconnected(error_code, ssl_socket);
                }
                ssl_socket->next_layer().close(error_code);
                if (on_socket_disconnected && error_code) {
                    on_socket_disconnected(error_code, ssl_socket);
                }
            }
            if (request_buffers.contains(ssl_socket))
                request_buffers.erase(ssl_socket);
            if (tcp.ssl_sockets.contains(ssl_socket))
                tcp.ssl_sockets.erase(ssl_socket);
            if (on_socket_disconnected) on_socket_disconnected(error, ssl_socket);
        }

        void process_response(Server::FResponse &res, const ssl_socket_ptr &ssl_socket) {
            std::string payload = "HTTP/" + res.version + " 200 OK\r\n";
            if (!res.headers.empty()) {
                for (const std::pair<std::string, std::string> header: res.headers) {
                    payload += header.first + ": " + header.second + "\r\n";
                }
                payload += "Content-Length: " + std::to_string(res.body.length()) + "\r\n";
            }
            payload += "\r\n";
            if (!res.body.empty()) payload += res.body + "\r\n";

            const bool should_close = res.headers.contains("Connection")
                                          ? (res.headers["Connection"] == "close")
                                          : true;

            asio::async_write(
                *ssl_socket, asio::buffer(payload.data(), payload.size()),
                std::bind(&HttpServerSsl::write_response, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, ssl_socket, should_close));
        }

        void process_error_response(const uint32_t status_code, const Server::FResponse &response,
                                    const ssl_socket_ptr &ssl_socket) {
            std::string payload = "HTTP/1.1 " + std::to_string(status_code) + " " + ResponseStatusCode.at(status_code) +
                                  "\r\n";
            if (response.headers.size() > 0) {
                for (const std::pair<std::string, std::string> header: response.headers) {
                    payload += header.first + ": " + header.second + "\r\n";
                }
                if (!response.headers.contains("Content-Length")) {
                    payload += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
                }
            }
            payload += "\r\n";
            if (response.body.size() > 0) {
                payload += response.body + "\r\n";
            }
            asio::async_write(
                *ssl_socket, asio::buffer(payload.data(), payload.size()),
                std::bind(&HttpServerSsl::write_response, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, ssl_socket, true));
        }

        void consume_request_buffer(const ssl_socket_ptr &ssl_socket) {
            if (!request_buffers.contains(ssl_socket))
                return;
            const std::shared_ptr<asio::streambuf> streambuf = request_buffers[ssl_socket];
            const size_t size = streambuf->size();
            if (size > 0)
                streambuf->consume(size);
        }

        void run_context_thread() {
            std::lock_guard<std::mutex> guard(mutex_io);
            error_code.clear();
            ssl_socket_ptr conn_ssl_socket = std::make_shared<asio::ssl::stream<asio::ip::tcp::socket> >(
                tcp.context, tcp.ssl_context);
            tcp.acceptor.async_accept(
                conn_ssl_socket->lowest_layer(),
                std::bind(&HttpServerSsl::accept, this, asio::placeholders::error, conn_ssl_socket));
            tcp.context.run();
            if (!is_closing)
                close();
        }

        void accept(const asio::error_code &error, ssl_socket_ptr &ssl_socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
                if (tcp.acceptor.is_open()) {
                    ssl_socket_ptr conn_ssl_socket = std::make_shared<asio::ssl::stream<asio::ip::tcp::socket> >(
                        tcp.context, tcp.ssl_context);
                    tcp.acceptor.async_accept(
                        conn_ssl_socket->lowest_layer(),
                        std::bind(&HttpServerSsl::accept, this, asio::placeholders::error, conn_ssl_socket));
                }
                return;
            }

            if (tcp.ssl_sockets.size() < backlog) {
                ssl_socket->async_handshake(asio::ssl::stream_base::server,
                                            std::bind(&HttpServerSsl::ssl_handshake, this,
                                                      asio::placeholders::error, ssl_socket));
            } else {
                std::lock_guard<std::mutex> guard(mutex_error);
                if (is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
            }

            if (tcp.acceptor.is_open()) {
                ssl_socket_ptr conn_ssl_socket = std::make_shared<asio::ssl::stream<asio::ip::tcp::socket> >(
                    tcp.context, tcp.ssl_context);
                tcp.acceptor.async_accept(
                    conn_ssl_socket->lowest_layer(),
                    std::bind(&HttpServerSsl::accept, this, asio::placeholders::error, conn_ssl_socket));
            }
        }

        void ssl_handshake(const asio::error_code &error,
                           ssl_socket_ptr &ssl_socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
                return;
            }

            tcp.ssl_sockets.insert(ssl_socket);
            std::shared_ptr<asio::streambuf> request_buffer = std::make_shared<asio::streambuf>();
            request_buffers.insert_or_assign(ssl_socket, request_buffer);
            asio::async_read_until(*ssl_socket, *request_buffer, "\r\n",
                                   std::bind(&HttpServerSsl::read_status_line, this,
                                             asio::placeholders::error, asio::placeholders::bytes_transferred,
                                             ssl_socket));

            if (on_socket_accepted) on_socket_accepted(ssl_socket);
        }

        void write_response(const asio::error_code &error, const size_t bytes_sent, const ssl_socket_ptr &ssl_socket,
                            const bool close_connection) {
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_response_sent) on_response_sent(error, ssl_socket);
                if (!is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
                return;
            }
            if (on_bytes_transfered) on_bytes_transfered(bytes_sent, 0);
            if (close_connection)
                disconnect_socket(ssl_socket);
            else {
                asio::async_read_until(*ssl_socket, *request_buffers.at(ssl_socket), "\r\n",
                                       std::bind(&HttpServerSsl::read_status_line, this,
                                                 asio::placeholders::error, asio::placeholders::bytes_transferred,
                                                 ssl_socket));
            }

            if (on_response_sent) on_response_sent(error, ssl_socket);
        }

        void read_status_line(const asio::error_code &error, const size_t bytes_recvd,
                              const ssl_socket_ptr &ssl_socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
                return;
            }
            if (on_bytes_transfered) on_bytes_transfered(0, bytes_recvd);
            std::istream response_stream(request_buffers.at(ssl_socket).get());
            std::string method;
            response_stream >> method;
            std::string path;
            response_stream >> path;
            std::string version;
            response_stream >> version;

            std::string error_payload;
            std::set<std::string> versions = {"HTTP/1.0", "HTTP/1.1", "HTTP/2.0"};
            if (!Server::RequestMethod.contains(method)) {
                Server::FResponse response;
                if (!versions.contains(version)) {
                    version = "1.1";
                } else {
                    version.erase(0, 5);
                }
                response.version = version;
                response.headers.insert_or_assign("Allow", "DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT, TRACE");
                response.headers.insert_or_assign("Content-Type", "text/plain");
                response.headers.insert_or_assign("Connection", "close");
                response.body = "Method \"" + method + "\" not allowed";
                process_error_response(405, response, ssl_socket);
                return;
            }
            if (!versions.contains(version)) {
                Server::FResponse response;
                response.version = "1.1";
                response.headers.insert_or_assign("Content-Type", "text/plain");
                response.headers.insert_or_assign("Connection", "close");
                response.body = "The server does not support the HTTP version used in the request.";
                process_error_response(505, response, ssl_socket);
                return;
            }

            Server::FRequest request;
            version.erase(0, 5);
            request.version = version;
            request.method = verb.at(method);
            request.path = path;

            request_buffers.at(ssl_socket)->consume(2);
            asio::async_read_until(
                *ssl_socket, *request_buffers.at(ssl_socket), "\r\n\r\n",
                std::bind(&HttpServerSsl::read_headers, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, request, ssl_socket));
        }

        void read_headers(const asio::error_code &error, const size_t bytes_recvd, Server::FRequest &request,
                          const ssl_socket_ptr &ssl_socket) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (!is_closing)
                    disconnect_socket_after_error(error, ssl_socket);
                return;
            }
            std::istream response_stream(&*request_buffers.at(ssl_socket));
            std::string header;
            std::cout << header << std::endl;

            while (std::getline(response_stream, header) && header != "\r")
                Server::req_append_header(request, header);

            read_body(request, ssl_socket);
        }

        void read_body(Server::FRequest &request, const ssl_socket_ptr &ssl_socket) {
            if (request_buffers.at(ssl_socket)->size() == 0) {
                if (on_request_received) {
                    Server::FResponse response;
                    response.version = request.version;
                    response.headers = headers;
                    if (request.headers.contains("Connection"))
                        response.headers["Connection"] = request.headers["Connection"][0];
                    on_request_received(request, response, ssl_socket);
                    consume_request_buffer(ssl_socket);
                }
                return;
            }

            std::ostringstream body_buffer;
            body_buffer << request_buffers.at(ssl_socket);
            Server::req_set_body(request, body_buffer.str());

            if (on_request_received) {
                Server::FResponse response;
                response.version = request.version;
                response.headers = headers;
                if (request.headers.contains("Connection"))
                    response.headers["Connection"] = request.headers["Connection"][0];
                on_request_received(request, response, ssl_socket);
            }
            consume_request_buffer(ssl_socket);
        }
    };
#endif
}
