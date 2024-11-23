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

#include <IP/Net/Common.hpp>

namespace InternetProtocol {
    class HttpClient {
    public:
        HttpClient() {
            request.headers.insert_or_assign("Accept", "*/*");
            request.headers.insert_or_assign("User-Agent", "ASIO 2.30.2");
            request.headers.insert_or_assign("Connection", "close");
        }

        ~HttpClient() {
            should_stop_context = true;
            asio::error_code ec;
            tcp.context.stop();
            if (!tcp.context.stopped() || tcp.socket.is_open()) {
                cancel_request();
            }
            thread_pool->wait();
            clear_request();
            clear_payload();
            clear_response();
        }

        /*HTTP SETTINGS*/
        void set_host(const std::string &url = "localhost",
                     const std::string &port = "") {
            host = url;
            service = port;
        }

        std::string get_host() const { return host; }
        std::string get_port() const { return service; }

        void set_timeout(uint8_t value = 3) { timeout = value; }
        uint8_t get_timeout() const { return timeout; }

        void set_max_attemp(uint8_t value = 3) { max_attemp = value; }
        uint8_t get_max_attemp() const { return timeout; }

        /*REQUEST DATA*/
        void set_request(const FRequest &value) { request = value; }
        FRequest get_request() const { return request; }

        void set_request_method(EVerb requestMethod = EVerb::GET) {
            request.verb = requestMethod;
        }

        EVerb get_request_method() const { return request.verb; }

        void set_version(const std::string &version = "1.1") {
            request.version = version;
        }

        std::string get_version() const { return request.version; }

        void set_path(const std::string &path = "/") {
            request.path = path.empty() ? "/" : path;
        }

        std::string get_path() const { return request.path; }

        void append_params(const std::string &key, const std::string &value) {
            request.params[key] = value;
        }

        void clear_params() { request.params.clear(); }
        void remove_param(const std::string &key) { request.params.erase(key); }

        bool has_param(const std::string &key) const {
            return request.params.find(key) != request.params.end();
        }

        std::map<std::string, std::string> get_params() const {
            return request.params;
        }

        void append_headers(const std::string &key, const std::string &value) {
            request.headers[key] = value;
        }

        void clear_headers() { request.headers.clear(); }
        void remove_header(const std::string &key) { request.headers.erase(key); }

        bool has_header(const std::string &key) const {
            return request.headers.find(key) != request.headers.end();
        }

        std::map<std::string, std::string> get_headers() const {
            return request.headers;
        }

        void set_body(const std::string &value) { request.body = value; }
        void clear_body() { request.body.clear(); }
        std::string get_body() const { return request.body; }

        FRequest get_request_data() const { return request; }

        /*PAYLOAD*/
        void prepare_payload() {
            if (!payload.empty()) payload.clear();

            payload = verb.at(request.verb) + " " + request.path;

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
            if (!thread_pool) return;
            asio::post(*thread_pool, [this]() {
                mutex_payload.lock();
                prepare_payload();
                if (on_async_payload_finished) on_async_payload_finished();
                mutex_payload.unlock();
            });
        }

        std::string get_payload_data() const { return payload; }

        /*RESPONSE DATA*/
        FResponse get_response_data() const { return response; }

        /*CONNECTION*/
        bool process_request() {
            if (!thread_pool && !payload.empty()) return false;

            asio::post(*thread_pool, std::bind(&HttpClient::run_context_thread, this));
            return true;
        }

        void cancel_request() {
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
            if (on_request_canceled)
                on_request_canceled();
        }

        /*MEMORY MANAGER*/
        void clear_request() { request.clear(); }
        void clear_payload() { payload.clear(); }
        void clear_response() { response.clear(); }

        /*ERRORS*/
        int get_error_code() const { return tcp.error_code.value(); }
        std::string get_error_message() const { return tcp.error_code.message(); }

        /*EVENTS*/
        std::function<void()> on_async_payload_finished;
        std::function<void(const FResponse)> on_request_complete;
        std::function<void()> on_request_canceled;
        std::function<void(const size_t, const size_t)> on_request_progress;
        std::function<void(const int)> on_request_will_retry;
        std::function<void(const asio::error_code &)> on_request_fail;
        std::function<void(const int)> on_response_fail;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::unique_ptr<asio::thread_pool> thread_pool =
                std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutex_payload;
        std::mutex mutex_io;
        bool should_stop_context = false;
        std::string host = "localhost";
        std::string service;
        uint8_t timeout = 3;
        uint8_t max_attemp = 3;
        FRequest request;
        FAsioTcp tcp;
        std::string payload;
        asio::streambuf request_buffer;
        asio::streambuf response_buffer;
        FResponse response;
        const std::map<EVerb, std::string> verb = {
            {EVerb::GET, "GET"}, {EVerb::POST, "POST"},
            {EVerb::PUT, "PUT"}, {EVerb::PATCH, "PATCH"},
            {EVerb::DEL, "DELETE"}, {EVerb::COPY, "COPY"},
            {EVerb::HEAD, "HEAD"}, {EVerb::OPTIONS, "OPTIONS"},
            {EVerb::LOCK, "LOCK"}, {EVerb::UNLOCK, "UNLOCK"},
            {EVerb::PROPFIND, "PROPFIND"},
        };

        void run_context_thread() {
            mutex_io.lock();
            while (tcp.attemps_fail <= max_attemp && !should_stop_context) {
                if (on_request_will_retry && tcp.attemps_fail > 0)
                    on_request_will_retry(tcp.attemps_fail);
                tcp.error_code.clear();
                tcp.resolver.async_resolve(
                    get_host(), get_port(),
                    std::bind(&HttpClient::resolve, this, asio::placeholders::error,
                              asio::placeholders::results));
                tcp.context.run();
                tcp.context.restart();
                if (!tcp.error_code) break;
                tcp.attemps_fail++;
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
            }
            consume_stream_buffers();
            mutex_io.unlock();
        }

        void consume_stream_buffers() {
            request_buffer.consume(request_buffer.size());
            response_buffer.consume(request_buffer.size());
        }

        void resolve(const asio::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                tcp.error_code = error;
                if (on_request_fail)
                    on_request_fail(error);
                return;
            }
            tcp.endpoints = endpoints;
            asio::async_connect(
                tcp.socket, tcp.endpoints,
                std::bind(&HttpClient::connect, this, asio::placeholders::error));
        }

        void connect(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (on_request_fail)
                    on_request_fail(error);
                return;
            }
            std::ostream request_stream(&request_buffer);
            request_stream << payload;
            asio::async_write(
                tcp.socket, request_buffer,
                std::bind(&HttpClient::write_request, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
        }

        void write_request(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                tcp.error_code = error;
                if (on_request_fail)
                    on_request_fail(error);
                return;
            }
            if (on_request_progress) on_request_progress(bytes_sent, 0);
            asio::async_read_until(tcp.socket, response_buffer, "\r\n",
                                   std::bind(&HttpClient::read_status_line, this,
                                             asio::placeholders::error, bytes_sent,
                                             asio::placeholders::bytes_transferred));
        }

        void read_status_line(const asio::error_code &error, const size_t bytes_sent,
                              const size_t bytes_recvd) {
            if (error) {
                tcp.error_code = error;
                if (on_request_fail)
                    on_request_fail(error);
                return;
            }
            if (on_request_progress) on_request_progress(bytes_sent, bytes_recvd);
            std::istream response_stream(&response_buffer);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                if (on_response_fail) on_response_fail(-1);
                return;
            }
            if (status_code != 200) {
                if (on_response_fail) on_response_fail(status_code);
                return;
            }
            asio::async_read_until(
                tcp.socket, response_buffer, "\r\n\r\n",
                std::bind(&HttpClient::read_headers, this, asio::placeholders::error));
        }

        void read_headers(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (on_request_fail)
                    on_request_fail(error);
                return;
            }
            response.clear();
            std::istream response_stream(&response_buffer);
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                response.appendHeader(header);
            std::ostringstream content_buffer;
            if (response_buffer.size() > 0) {
                content_buffer << &response_buffer;
                response.setContent(content_buffer.str());
            }
            asio::async_read(
                tcp.socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&HttpClient::read_content, this, asio::placeholders::error));
        }

        void read_content(const asio::error_code &error) {
            if (error) {
                if (on_request_complete) on_request_complete(response);
                return;
            }
            std::ostringstream stream_buffer;
            stream_buffer << &response_buffer;
            if (!stream_buffer.str().empty()) response.setContent(stream_buffer.str());
            asio::async_read(
                tcp.socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&HttpClient::read_content, this, asio::placeholders::error));
        }
    };
#ifdef ASIO_USE_OPENSSL
    class HttpClientSsl {
    public:
        HttpClientSsl() {
            request.headers.insert_or_assign("Accept", "*/*");
            request.headers.insert_or_assign("User-Agent", "ASIO 2.30.2");
            request.headers.insert_or_assign("Connection", "close");
        }

        ~HttpClientSsl() {
            should_stop_context = true;
            asio::error_code ec;
            tcp.resolver.cancel();
            if (!tcp.context.stopped() || tcp.ssl_socket.lowest_layer().is_open()) {
                cancel_request();
            }
            thread_pool->wait();
            clear_request();
            clear_payload();
            clear_response();
        }

        /*HTTP SETTINGS*/
        void set_host(const std::string &url = "localhost",
                     const std::string &port = "") {
            host = url;
            service = port;
        }

        std::string get_host() const { return host; }
        std::string get_port() const { return service; }

        void set_timeout(uint8_t value = 3) { timeout = value; }
        uint8_t get_timeout() const { return timeout; }

        void set_max_attemp(uint8_t value = 3) { max_attemp = value; }
        uint8_t get_max_attemp() const { return timeout; }

        /*REQUEST DATA*/
        void set_request(const FRequest &value) { request = value; }
        FRequest get_request() const { return request; }

        void set_request_method(EVerb requestMethod = EVerb::GET) {
            request.verb = requestMethod;
        }

        EVerb get_request_method() const { return request.verb; }

        void set_version(const std::string &version = "2.0") {
            request.version = version;
        }

        std::string get_version() const { return request.version; }

        void set_path(const std::string &path = "/") {
            request.path = path.empty() ? "/" : path;
        }

        std::string get_path() const { return request.path; }

        void append_params(const std::string &key, const std::string &value) {
            request.params[key] = value;
        }

        void clear_params() { request.params.clear(); }
        void remove_param(const std::string &key) { request.params.erase(key); }

        bool has_param(const std::string &key) const {
            return request.params.find(key) != request.params.end();
        }

        std::map<std::string, std::string> get_params() const {
            return request.params;
        }

        void append_headers(const std::string &key, const std::string &value) {
            request.headers[key] = value;
        }

        void clear_headers() { request.headers.clear(); }
        void remove_header(const std::string &key) { request.headers.erase(key); }

        bool has_header(const std::string &key) const {
            return request.headers.find(key) != request.headers.end();
        }

        std::map<std::string, std::string> get_headers() const {
            return request.headers;
        }

        void set_body(const std::string &value) { request.body = value; }
        void clear_body() { request.body.clear(); }
        std::string get_body() const { return request.body; }

        FRequest get_request_data() const { return request; }

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

        /*PAYLOAD*/
        void prepare_payload() {
            if (!payload.empty()) payload.clear();

            payload = verb.at(request.verb) + " " + request.path;

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
            asio::post(*thread_pool, [this]() {
                mutex_payload.lock();
                prepare_payload();
                if (on_async_payload_finished) on_async_payload_finished();
                mutex_payload.unlock();
            });
        }

        std::string get_payload_data() const { return payload; }

        /*RESPONSE DATA*/
        FResponse get_response_data() const { return response; }

        /*CONNECTION*/
        bool process_request() {
            if (!thread_pool && !payload.empty()) return false;

            asio::post(*thread_pool, std::bind(&HttpClientSsl::run_context_thread, this));
            return true;
        }

        void cancel_request() {
            tcp.context.stop();
            asio::error_code ec_shutdown;
            asio::error_code ec_close;
            tcp.ssl_socket.shutdown(ec_shutdown);
            tcp.ssl_socket.lowest_layer().close(ec_close);
            if (should_stop_context) return;
            if (ec_shutdown && on_error)
                on_error(ec_shutdown);
            tcp.ssl_socket.lowest_layer().close(ec_close);
            if (ec_close && on_error)
                on_error(ec_close);
            if (on_request_canceled) on_request_canceled();
        }

        /*MEMORY MANAGER*/
        void clear_request() { request.clear(); }
        void clear_payload() { payload.clear(); }
        void clear_response() { response.clear(); }

        /*ERRORS*/
        int get_error_code() const { return tcp.error_code.value(); }
        std::string get_error_message() const { return tcp.error_code.message(); }

        /*EVENTS*/
        std::function<void()> on_async_payload_finished;
        std::function<void(const FResponse)> on_request_complete;
        std::function<void()> on_request_canceled;
        std::function<void(const size_t, const size_t)> on_request_progress;
        std::function<void(const int)> on_request_will_retry;
        std::function<void(const asio::error_code &)> on_request_fail;
        std::function<void(const int)> on_response_fail;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::unique_ptr<asio::thread_pool> thread_pool =
                std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutex_payload;
        std::mutex mutex_io;
        bool should_stop_context = false;
        std::string host = "localhost";
        std::string service;
        uint8_t timeout = 3;
        uint8_t max_attemp = 3;
        FRequest request;
        FAsioTcpSsl tcp;
        std::string payload;
        asio::streambuf request_buffer;
        asio::streambuf response_buffer;
        FResponse response;
        const std::map<EVerb, std::string> verb = {
            {EVerb::GET, "GET"}, {EVerb::POST, "POST"},
            {EVerb::PUT, "PUT"}, {EVerb::PATCH, "PATCH"},
            {EVerb::DEL, "DELETE"}, {EVerb::COPY, "COPY"},
            {EVerb::HEAD, "HEAD"}, {EVerb::OPTIONS, "OPTIONS"},
            {EVerb::LOCK, "LOCK"}, {EVerb::UNLOCK, "UNLOCK"},
            {EVerb::PROPFIND, "PROPFIND"},
        };

        void run_context_thread() {
            mutex_io.lock();
            while (tcp.attemps_fail <= max_attemp && !should_stop_context) {
                if (on_request_will_retry && tcp.attemps_fail > 0)
                    on_request_will_retry(tcp.attemps_fail);
                tcp.error_code.clear();
                tcp.context.restart();
                tcp.resolver.async_resolve(
                    get_host(), get_port(),
                    std::bind(&HttpClientSsl::resolve, this, asio::placeholders::error,
                              asio::placeholders::results));
                tcp.context.run();
                if (!tcp.error_code) break;
                tcp.attemps_fail++;
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
            }
            consume_stream_buffers();
            mutex_io.unlock();
        }

        void consume_stream_buffers() {
            request_buffer.consume(request_buffer.size());
            response_buffer.consume(request_buffer.size());
        }

        void resolve(const asio::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                tcp.error_code = error;
                if (on_request_fail)
                    on_request_fail(error);
                return;
            }
            tcp.endpoints = endpoints;
            asio::async_connect(
                tcp.ssl_socket.lowest_layer(), tcp.endpoints,
                std::bind(&HttpClientSsl::connect, this, asio::placeholders::error));
        }

        void connect(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (on_request_fail)
                    on_request_fail(error);
                return;
            }
            tcp.ssl_socket.async_handshake(asio::ssl::stream_base::client,
                                           std::bind(&HttpClientSsl::ssl_handshake,
                                                     this, asio::placeholders::error));
        }

        void ssl_handshake(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (on_request_fail)
                    on_request_fail(error);
                return;
            }
            std::ostream request_stream(&request_buffer);
            request_stream << payload;
            asio::async_write(tcp.ssl_socket, request_buffer,
                              std::bind(&HttpClientSsl::write_request, this,
                                        asio::placeholders::error,
                                        asio::placeholders::bytes_transferred));
        }

        void write_request(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                tcp.error_code = error;
                if (on_request_fail)
                    on_request_fail(error);
                return;
            }
            if (on_request_progress) on_request_progress(bytes_sent, 0);
            asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n",
                                   std::bind(&HttpClientSsl::read_status_line, this,
                                             asio::placeholders::error, bytes_sent,
                                             asio::placeholders::bytes_transferred));
        }

        void read_status_line(const asio::error_code &error, const size_t bytes_sent,
                              const size_t bytes_recvd) {
            if (error) {
                tcp.error_code = error;
                if (on_request_fail)
                    on_request_fail(error);
                return;
            }
            if (on_request_progress) on_request_progress(bytes_sent, bytes_recvd);
            std::istream response_stream(&response_buffer);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                if (on_response_fail) on_response_fail(-1);
                return;
            }
            if (status_code != 200) {
                if (on_response_fail) on_response_fail(status_code);
                return;
            }
            asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n\r\n",
                                   std::bind(&HttpClientSsl::read_headers, this,
                                             asio::placeholders::error));
        }

        void read_headers(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (on_request_fail)
                    on_request_fail(error);
                return;
            }
            response.clear();
            std::istream response_stream(&response_buffer);
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                response.appendHeader(header);
            std::ostringstream content_buffer;
            if (response_buffer.size() > 0) {
                content_buffer << &response_buffer;
                response.setContent(content_buffer.str());
            }
            asio::async_read(tcp.ssl_socket, response_buffer,
                             asio::transfer_at_least(1),
                             std::bind(&HttpClientSsl::read_content, this,
                                       asio::placeholders::error));
        }

        void read_content(const asio::error_code &error) {
            if (error) {
                if (on_request_complete) on_request_complete(response);
                return;
            }
            std::ostringstream stream_buffer;
            stream_buffer << &response_buffer;
            if (!stream_buffer.str().empty()) response.setContent(stream_buffer.str());
            asio::async_read(tcp.ssl_socket, response_buffer,
                             asio::transfer_at_least(1),
                             std::bind(&HttpClientSsl::read_content, this,
                                       asio::placeholders::error));
        }
    };
#endif
} // namespace InternetProtocol
