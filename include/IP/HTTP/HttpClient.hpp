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

#include <iostream>
#include <IP/Net/Common.hpp>
#include "IP/Net/Utils.hpp"

namespace InternetProtocol {
    class HttpClient {
    public:
        HttpClient() {
            request.headers.insert_or_assign("Accept", "*/*");
            request.headers.insert_or_assign("User-Agent", "ASIO");
            request.headers.insert_or_assign("Connection", "close");
        }

        ~HttpClient() {
            if (get_socket().is_open()) close();
        }

        /*HTTP SETTINGS*/
        void set_host(const std::string &url = "localhost", const std::string &port = "3000") {
            host = url;
            service = port;
        }

        asio::ip::tcp::socket &get_socket() { return tcp.socket; }

        /*REQUEST DATA*/
        void set_request(const Client::FRequest &value) { request = value; }
        Client::FRequest get_request() const { return request; }

        void set_request_method(EMethod requestMethod = EMethod::GET) {
            request.method = requestMethod;
        }

        EMethod get_request_method() const { return request.method; }

        void set_version(const std::string &version = "1.1") {
            request.version = version;
        }

        std::string get_version() const { return request.version; }

        void set_path(const std::string &path = "/") {
            request.path = path.empty() ? "/" : path;
        }

        std::string get_path() const { return request.path; }

        void set_params(const std::map<std::string, std::string> &params) {
            request.params = params;
        }

        std::map<std::string, std::string> &get_params() {
            return request.params;
        }

        void set_headers(const std::map<std::string, std::string> &headers) {
            request.headers = headers;
        }

        std::map<std::string, std::string> &get_headers() {
            return request.headers;
        }

        void set_body(const std::string &value) { request.body = value; }
        std::string &get_body() { return request.body; }

        Client::FRequest get_request_data() const { return request; }

        /*PAYLOAD*/
        void prepare_payload() {
            if (!payload.empty()) payload.clear();

            payload = Client::RequestMethod.at(request.method) + " " + request.path;

            if (!request.params.empty()) {
                payload += "?";
                bool first = true;
                for (const std::pair<std::string, std::string> param: request.params) {
                    if (!first) payload += "&";
                    payload += param.first + "=" + param.second;
                    first = false;
                }
            }
            payload += " HTTP/" + request.version + "\r\n";

            payload += "Host: " + host;
            if (!service.empty()) payload += ":" + service;
            payload += "\r\n";

            if (!request.headers.empty()) {
                for (const std::pair<std::string, std::string> header: request.headers) {
                    payload += header.first + ": " + header.second + "\r\n";
                }
                payload +=
                        "Content-Length: " + std::to_string(request.body.length()) + "\r\n";
            }
            payload += "\r\n";

            if (!request.body.empty()) payload += "\r\n" + request.body;
        }

        void async_prepare_payload() {
            asio::post(thread_pool, [this]() {
                std::lock_guard<std::mutex> guard(mutex_payload);
                prepare_payload();
                if (on_async_payload_finished) on_async_payload_finished();
            });
        }

        std::string get_payload_data() const { return payload; }

        /*RESPONSE DATA*/
        Client::FResponse get_response_data() const { return response; }

        /*CONNECTION*/
        bool process_request() {
            if (payload.empty()) return false;

            asio::post(thread_pool, std::bind(&HttpClient::run_context_thread, this));
            return true;
        }

        void close() {
            is_closing = true;
            tcp.context.stop();
            if (get_socket().is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                tcp.socket.shutdown(asio::ip::udp::socket::shutdown_both, error_code);
                if (error_code && on_error) {
                    on_error(error_code);
                }
                tcp.socket.close(error_code);
                if (error_code && on_error) {
                    on_error(error_code);
                }
            }
            tcp.context.restart();
            if (on_close) on_close();
            is_closing = false;
        }

        /*MEMORY MANAGER*/
        void clear_request() { Client::req_clear(request); }
        void clear_payload() { payload.clear(); }
        void clear_response() { Client::res_clear(response); }

        /*ERRORS*/
        asio::error_code get_error_code() const { return error_code; }

        /*EVENTS*/
        std::function<void()> on_async_payload_finished;
        std::function<void(const size_t, const size_t)> on_request_progress;
        std::function<void(const Client::FResponse)> on_request_complete;
        std::function<void(const Client::FResponse)> on_request_fail;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_payload;
        std::mutex mutex_io;
        std::mutex mutex_error;
        bool is_closing = false;
        Client::FAsioTcp tcp;
        asio::error_code error_code;
        std::string host = "localhost";
        std::string service = "3000";
        Client::FRequest request;
        std::string payload;
        asio::streambuf request_buffer;
        asio::streambuf response_buffer;
        Client::FResponse response;

        void consume_stream_buffers() {
            const size_t req_size = request_buffer.size();
            if (req_size > 0)
                request_buffer.consume(req_size);
            const size_t res_size = response_buffer.size();
            if (res_size > 0)
                response_buffer.consume(res_size);
        }

        void run_context_thread() {
            if (tcp.socket.is_open()) {
                std::ostream request_stream(&request_buffer);
                request_stream << payload;
                asio::async_write(
                    tcp.socket, request_buffer,
                    std::bind(&HttpClient::write_request, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred, false));
                return;
            }
            std::lock_guard<std::mutex> lock(mutex_io);
            error_code.clear();
            tcp.resolver.async_resolve(
                host, service,
                std::bind(&HttpClient::resolve, this, asio::placeholders::error,
                          asio::placeholders::endpoint));
            tcp.context.run();
            if (!is_closing)
                close();
        }

        void resolve(const asio::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            tcp.endpoints = endpoints;
            asio::async_connect(
                tcp.socket, tcp.endpoints,
                std::bind(&HttpClient::connect, this, asio::placeholders::error));
        }

        void connect(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            std::ostream request_stream(&request_buffer);
            request_stream << payload;
            asio::async_write(
                tcp.socket, request_buffer,
                std::bind(&HttpClient::write_request, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred, true));
        }

        void write_request(const asio::error_code &error, const size_t bytes_sent, const bool trigger_read_until) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }

            if (on_request_progress) on_request_progress(bytes_sent, 0);
            if (trigger_read_until) {
                asio::async_read_until(tcp.socket, response_buffer, "\r\n",
                                       std::bind(&HttpClient::read_status_line, this,
                                                 asio::placeholders::error, asio::placeholders::bytes_transferred));
            }
        }

        void read_status_line(const asio::error_code &error,
                              const size_t bytes_recvd) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            if (on_request_progress) on_request_progress(0, bytes_recvd);
            Client::res_clear(response);
            std::istream response_stream(&response_buffer);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                std::lock_guard<std::mutex> guard(mutex_error);
                response.status_code = 505;
                if (on_request_fail) on_request_fail(response);
                return;
            }
            response.status_code = status_code;
            if (status_code != 200 && response_buffer.size() == 0) {
                if (on_request_fail) on_request_fail(response);
                return;
            }
            asio::async_read_until(
                tcp.socket, response_buffer, "\r\n\r\n",
                std::bind(&HttpClient::read_headers, this, asio::placeholders::error));
        }

        void read_headers(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            std::istream response_stream(&response_buffer);
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                Client::res_append_header(response, header);
            std::ostringstream body_buffer;

            if (response_buffer.size() > 0) {
                body_buffer << &response_buffer;
                Client::res_set_body(response, body_buffer.str());
            }

            std::ostringstream stream_buffer;
            stream_buffer << &response_buffer;
            if (!stream_buffer.str().empty()) Client::res_append_body(response, stream_buffer.str());
            if (response_buffer.size() > 0) {
                asio::async_read(
                    tcp.socket, response_buffer, asio::transfer_at_least(1),
                    std::bind(&HttpClient::read_body, this, asio::placeholders::error));
                return;
            }
            if (response.status_code == 200) {
                if (on_request_complete) on_request_complete(response);
            } else {
                if (on_request_fail) on_request_fail(response);
            }
            consume_stream_buffers();
            if (response.headers["Connection"][0] == "close") {
                if (!is_closing) close();
            } else {
                asio::async_read_until(tcp.socket, response_buffer, "\r\n",
                                       std::bind(&HttpClient::read_status_line, this,
                                                 asio::placeholders::error, asio::placeholders::bytes_transferred));
            }
        }

        void read_body(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> guard(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }

            std::ostringstream body_buffer;
            body_buffer << &response_buffer;
            if (!body_buffer.str().empty()) Client::res_append_body(response, body_buffer.str());
            if (response_buffer.size() > 0) {
                asio::async_read(
                    tcp.socket, response_buffer, asio::transfer_at_least(1),
                    std::bind(&HttpClient::read_body, this, asio::placeholders::error));
                return;
            }
            if (response.status_code == 200) {
                if (on_request_complete) on_request_complete(response);
            } else {
                if (on_request_fail) on_request_fail(response);
            }
            consume_stream_buffers();
            if (response.headers["Connection"][0] == "close") {
                if (!is_closing) close();
            } else {
                asio::async_read_until(tcp.socket, response_buffer, "\r\n",
                                       std::bind(&HttpClient::read_status_line, this,
                                                 asio::placeholders::error, asio::placeholders::bytes_transferred));
            }
        }
    };
#ifdef ASIO_USE_OPENSSL
    class HttpClientSsl {
    public:
        HttpClientSsl() {
            request.headers.insert_or_assign("Accept", "*/*");
            request.headers.insert_or_assign("User-Agent", "ASIO");
            request.headers.insert_or_assign("Connection", "close");
        }

        ~HttpClientSsl() {
            if (get_ssl_socket().lowest_layer().is_open()) close();
        }

        /*HTTP SETTINGS*/
        void set_host(const std::string &url = "localhost", const std::string &port = "443") {
            host = url;
            service = port;
        }

        asio::ssl::context &get_ssl_context() { return tcp.ssl_context; }
        asio::ssl::stream<asio::ip::tcp::socket> &get_ssl_socket() { return tcp.ssl_socket; }

        void update_ssl_socket() {
            tcp.ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(tcp.context, tcp.ssl_context);
        }

        /*REQUEST DATA*/
        void set_request(const Client::FRequest &value) { request = value; }
        Client::FRequest get_request() const { return request; }

        void set_request_method(EMethod requestMethod = EMethod::GET) {
            request.method = requestMethod;
        }

        EMethod get_request_method() const { return request.method; }

        void set_version(const std::string &version = "2.0") {
            request.version = version;
        }

        std::string get_version() const { return request.version; }

        void set_path(const std::string &path = "/") {
            request.path = path.empty() ? "/" : path;
        }

        std::string get_path() const { return request.path; }

        void set_params(const std::map<std::string, std::string> &params) {
            request.params = params;
        }

        std::map<std::string, std::string> &get_params() {
            return request.params;
        }

        void set_headers(const std::map<std::string, std::string> &headers) {
            request.headers = headers;
        }

        std::map<std::string, std::string> &get_headers() {
            return request.headers;
        }

        void set_body(const std::string &value) { request.body = value; }
        std::string &get_body() { return request.body; }

        Client::FRequest get_request_data() const { return request; }

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

        /*PAYLOAD*/
        void prepare_payload() {
            if (!payload.empty()) payload.clear();

            payload = Client::RequestMethod.at(request.method) + " " + request.path;

            if (!request.params.empty()) {
                payload += "?";
                bool first = true;
                for (const std::pair<std::string, std::string> param: request.params) {
                    if (!first) payload += "&";
                    payload += param.first + "=" + param.second;
                    first = false;
                }
            }
            payload += " HTTP/" + request.version + "\r\n";

            payload += "Host: " + host;
            if (!service.empty()) payload += ":" + service;
            payload += "\r\n";

            if (!request.headers.empty()) {
                for (const std::pair<std::string, std::string> header: request.headers) {
                    payload += header.first + ": " + header.second + "\r\n";
                }
                payload +=
                        "Content-Length: " + std::to_string(request.body.length()) + "\r\n";
            }
            payload += "\r\n";

            if (!request.body.empty()) payload += "\r\n" + request.body;
        }

        void async_prepare_payload() {
            asio::post(thread_pool, [this]() {
                std::lock_guard<std::mutex> guard(mutex_payload);
                prepare_payload();
                if (on_async_payload_finished) on_async_payload_finished();
            });
        }

        std::string get_payload_data() const { return payload; }

        /*RESPONSE DATA*/
        Client::FResponse get_response_data() const { return response; }

        /*CONNECTION*/
        bool process_request() {
            if (payload.empty()) return false;

            asio::post(thread_pool, std::bind(&HttpClientSsl::run_context_thread, this));
            return true;
        }

        void close() {
            is_closing = true;
            if (get_ssl_socket().next_layer().is_open()) {
                std::lock_guard<std::mutex> guard(mutex_error);
                tcp.ssl_socket.shutdown(error_code);
                if (error_code && on_error) {
                    on_error(error_code);
                }
                tcp.ssl_socket.next_layer().close(error_code);
                if (error_code && on_error) {
                    on_error(error_code);
                }
            }
            tcp.context.stop();
            tcp.context.restart();
            tcp.ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(tcp.context, tcp.ssl_context);
            if (on_close) on_close();
            is_closing = false;
        }

        /*MEMORY MANAGER*/
        void clear_request() { Client::req_clear(request); }
        void clear_payload() { payload.clear(); }
        void clear_response() { Client::res_clear(response); }

        /*ERRORS*/
        asio::error_code get_error_code() const { return error_code; }

        /*EVENTS*/
        std::function<void()> on_async_payload_finished;
        std::function<void(const size_t, const size_t)> on_request_progress;
        std::function<void(const Client::FResponse)> on_request_complete;
        std::function<void(const Client::FResponse)> on_request_fail;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_payload;
        std::mutex mutex_io;
        std::mutex mutex_error;
        bool is_closing = false;
        Client::FAsioTcpSsl tcp;
        asio::error_code error_code;
        std::string host = "localhost";
        std::string service = "3000";
        Client::FRequest request;
        std::string payload;
        asio::streambuf request_buffer;
        asio::streambuf response_buffer;
        Client::FResponse response;

        void consume_stream_buffers() {
            const size_t req_size = request_buffer.size();
            if (req_size > 0)
                request_buffer.consume(req_size);
            const size_t res_size = response_buffer.size();
            if (res_size > 0)
                response_buffer.consume(res_size);
        }

        void run_context_thread() {
            if (get_ssl_socket().next_layer().is_open()) {
                std::ostream request_stream(&request_buffer);
                request_stream << payload;
                asio::async_write(tcp.ssl_socket, request_buffer,
                                  std::bind(&HttpClientSsl::write_request, this,
                                            asio::placeholders::error,
                                            asio::placeholders::bytes_transferred, false));
                return;
            }
            std::lock_guard<std::mutex> lock(mutex_io);
            error_code.clear();
            tcp.resolver.async_resolve(
                host, service,
                std::bind(&HttpClientSsl::resolve, this, asio::placeholders::error,
                          asio::placeholders::results));
            tcp.context.run();
            tcp.context.restart();
            if (!is_closing) {
                close();
            }
        }

        void resolve(const asio::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            tcp.endpoints = endpoints;
            asio::async_connect(
                tcp.ssl_socket.lowest_layer(), tcp.endpoints,
                std::bind(&HttpClientSsl::connect, this, asio::placeholders::error));
        }

        void connect(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            tcp.ssl_socket.async_handshake(asio::ssl::stream_base::client,
                                           std::bind(&HttpClientSsl::ssl_handshake,
                                                     this, asio::placeholders::error));
        }

        void ssl_handshake(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            std::ostream request_stream(&request_buffer);
            request_stream << payload;
            asio::async_write(tcp.ssl_socket, request_buffer,
                              std::bind(&HttpClientSsl::write_request, this,
                                        asio::placeholders::error,
                                        asio::placeholders::bytes_transferred, true));
        }

        void write_request(const asio::error_code &error, const size_t bytes_sent, const bool trigger_read_until) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            if (on_request_progress) on_request_progress(bytes_sent, 0);
            if (trigger_read_until) {
                asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n",
                                       std::bind(&HttpClientSsl::read_status_line, this,
                                                 asio::placeholders::error, asio::placeholders::bytes_transferred));
            }
        }

        void read_status_line(const asio::error_code &error,
                              const size_t bytes_recvd) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            if (on_request_progress) on_request_progress(0, bytes_recvd);
            Client::res_clear(response);
            std::istream response_stream(&response_buffer);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                std::lock_guard<std::mutex> lock(mutex_error);
                response.status_code = 505;
                if (on_request_fail) on_request_fail(response);
                return;
            }
            response.status_code = status_code;
            if (status_code != 200 && response_buffer.size() == 0) {
                if (on_request_fail) on_request_fail(response);
                return;
            }
            asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n\r\n",
                                   std::bind(&HttpClientSsl::read_headers, this,
                                             asio::placeholders::error));
        }

        void read_headers(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }
            std::istream response_stream(&response_buffer);
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                Client::res_append_header(response, header);
            std::ostringstream body_buffer;
            if (response_buffer.size() > 0) {
                body_buffer << &response_buffer;
                Client::res_set_body(response, body_buffer.str());
            }

            std::ostringstream stream_buffer;
            stream_buffer << &response_buffer;
            if (!stream_buffer.str().empty()) Client::res_append_body(response, stream_buffer.str());;
            if (response_buffer.size() > 0) {
                asio::async_read(tcp.ssl_socket, response_buffer,
                                 asio::transfer_at_least(1),
                                 std::bind(&HttpClientSsl::read_body, this,
                                           asio::placeholders::error));
                return;
            }
            if (response.status_code == 200) {
                if (on_request_complete) on_request_complete(response);
            } else {
                if (on_request_fail) on_request_fail(response);
            }
            consume_stream_buffers();
            if (response.headers["Connection"][0] == "close") {
                if (!is_closing) close();
            } else {
                asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n",
                                       std::bind(&HttpClientSsl::read_status_line, this,
                                                 asio::placeholders::error, asio::placeholders::bytes_transferred));
            }
        }

        void read_body(const asio::error_code &error) {
            if (error) {
                std::lock_guard<std::mutex> lock(mutex_error);
                error_code = error;
                if (on_error) on_error(error);
                return;
            }

            std::ostringstream body_buffer;
            body_buffer << &response_buffer;
            if (!body_buffer.str().empty()) Client::res_append_body(response, body_buffer.str());;
            if (response_buffer.size() > 0) {
                asio::async_read(tcp.ssl_socket, response_buffer,
                                 asio::transfer_at_least(1),
                                 std::bind(&HttpClientSsl::read_body, this,
                                           asio::placeholders::error));
                return;
            }
            if (response.status_code == 200) {
                if (on_request_complete) on_request_complete(response);
            } else {
                if (on_request_fail) on_request_fail(response);
            }
            consume_stream_buffers();
            if (response.headers["Connection"][0] == "close") {
                if (!is_closing) close();
            } else {
                asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n",
                                       std::bind(&HttpClientSsl::read_status_line, this,
                                                 asio::placeholders::error, asio::placeholders::bytes_transferred));
            }
        }
    };
#endif
} // namespace InternetProtocol
