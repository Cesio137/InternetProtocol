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
            tcp.context.stop();
            pool->stop();
            clearRequest();
            clearPayload();
            clearResponse();
        }

        /*HTTP SETTINGS*/
        void setHost(const std::string &url = "localhost",
                     const std::string &port = "") {
            host = url;
            service = port;
        }

        std::string getHost() const { return host; }
        std::string getPort() const { return service; }

        void setTimeout(uint8_t value = 3) { timeout = value; }
        uint8_t getTimeout() const { return timeout; }

        void setMaxAttemp(uint8_t value = 3) { maxAttemp = value; }
        uint8_t getMaxAttemp() const { return timeout; }

        /*REQUEST DATA*/
        void setRequest(const FRequest &value) { request = value; }
        FRequest getRequest() const { return request; }

        void setRequestMethod(EVerb requestMethod = EVerb::GET) {
            request.verb = requestMethod;
        }

        EVerb getRequestMethod() const { return request.verb; }

        void setVersion(const std::string &version = "2.0") {
            request.version = version;
        }

        std::string getVersion() const { return request.version; }

        void setPath(const std::string &path = "/") {
            request.path = path.empty() ? "/" : path;
        }

        std::string getPath() const { return request.path; }

        void appendParams(const std::string &key, const std::string &value) {
            request.params[key] = value;
        }

        void clearParams() { request.params.clear(); }
        void removeParam(const std::string &key) { request.params.erase(key); }

        bool hasParam(const std::string &key) const {
            return request.params.find(key) != request.params.end();
        }

        std::map<std::string, std::string> getParams() const {
            return request.params;
        }

        void AppendHeaders(const std::string &key, const std::string &value) {
            request.headers[key] = value;
        }

        void ClearHeaders() { request.headers.clear(); }
        void RemoveHeader(const std::string &key) { request.headers.erase(key); }

        bool hasHeader(const std::string &key) const {
            return request.headers.find(key) != request.headers.end();
        }

        std::map<std::string, std::string> GetHeaders() const {
            return request.headers;
        }

        void SetBody(const std::string &value) { request.body = value; }
        void ClearBody() { request.body.clear(); }
        std::string GetBody() const { return request.body; }

        FRequest getRequestData() const { return request; }

        /*PAYLOAD*/
        void preparePayload() {
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

        void async_preparePayload() {
            asio::post(*pool, [this]() {
                mutexPayload.lock();
                preparePayload();
                if (onAsyncPayloadFinished) onAsyncPayloadFinished();
                mutexPayload.unlock();
            });
        }

        std::string getPayloadData() const { return payload; }

        /*RESPONSE DATA*/
        FResponse getResponseData() const { return response; }

        /*CONNECTION*/
        bool processRequest() {
            if (!pool && !payload.empty()) return false;

            asio::post(*pool, std::bind(&HttpClient::run_context_thread, this));
            return true;
        }

        void cancelRequest() {
            tcp.context.stop();
            tcp.socket.shutdown(asio::ip::tcp::socket::shutdown_both, tcp.error_code);
            if (tcp.error_code && onError)
                onError(tcp.error_code.value(), tcp.error_code.message());
            tcp.socket.close(tcp.error_code);
            if (tcp.error_code && onError)
                onError(tcp.error_code.value(), tcp.error_code.message());
            if (onRequestCanceled) onRequestCanceled();
        }

        /*MEMORY MANAGER*/
        void clearRequest() { request.clear(); }
        void clearPayload() { payload.clear(); }
        void clearResponse() { response.clear(); }

        /*ERRORS*/
        int getErrorCode() const { return tcp.error_code.value(); }
        std::string getErrorMessage() const { return tcp.error_code.message(); }

        /*EVENTS*/
        std::function<void()> onAsyncPayloadFinished;
        std::function<void(const FResponse)> onRequestComplete;
        std::function<void()> onRequestCanceled;
        std::function<void(const size_t, const size_t)> onRequestProgress;
        std::function<void(const int)> onRequestWillRetry;
        std::function<void(const int, const std::string &)> onRequestFail;
        std::function<void(const int)> onResponseFail;
        std::function<void(const int, const std::string &)> onError;

    private:
        std::unique_ptr<asio::thread_pool> pool =
                std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutexPayload;
        std::mutex mutexIO;
        std::string host = "localhost";
        std::string service;
        uint8_t timeout = 3;
        uint8_t maxAttemp = 3;
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
            mutexIO.lock();
            while (tcp.attemps_fail <= maxAttemp) {
                if (onRequestWillRetry && tcp.attemps_fail > 0)
                    onRequestWillRetry(tcp.attemps_fail);
                tcp.error_code.clear();
                tcp.context.restart();
                tcp.resolver.async_resolve(
                    getHost(), getPort(),
                    std::bind(&HttpClient::resolve, this, asio::placeholders::error,
                              asio::placeholders::results));
                tcp.context.run();
                if (!tcp.error_code) break;
                tcp.attemps_fail++;
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
            }
            consume_stream_buffers();
            mutexIO.unlock();
        }

        void consume_stream_buffers() {
            request_buffer.consume(request_buffer.size());
            response_buffer.consume(request_buffer.size());
        }

        void resolve(const std::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                tcp.error_code = error;
                if (onRequestFail)
                    onRequestFail(tcp.error_code.value(), tcp.error_code.message());
                return;
            }
            // Attempt a connection to each endpoint in the list until we
            // successfully establish a connection.
            tcp.endpoints = endpoints;

            asio::async_connect(
                tcp.socket, tcp.endpoints,
                std::bind(&HttpClient::connect, this, asio::placeholders::error));
        }

        void connect(const std::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (onRequestFail)
                    onRequestFail(tcp.error_code.value(), tcp.error_code.message());
                return;
            }
            // The connection was successful. Send the request.;
            std::ostream request_stream(&request_buffer);
            request_stream << payload;
            asio::async_write(
                tcp.socket, request_buffer,
                std::bind(&HttpClient::write_request, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
        }

        void write_request(const std::error_code &error, const size_t bytes_sent) {
            if (error) {
                tcp.error_code = error;
                if (onRequestFail)
                    onRequestFail(tcp.error_code.value(), tcp.error_code.message());
                return;
            }
            // Read the response status line. The response_ streambuf will
            // automatically grow to accommodate the entire line. The growth may be
            // limited by passing a maximum size to the streambuf constructor.
            if (onRequestProgress) onRequestProgress(bytes_sent, 0);
            asio::async_read_until(tcp.socket, response_buffer, "\r\n",
                                   std::bind(&HttpClient::read_status_line, this,
                                             asio::placeholders::error, bytes_sent,
                                             asio::placeholders::bytes_transferred));
        }

        void read_status_line(const std::error_code &error, const size_t bytes_sent,
                              const size_t bytes_recvd) {
            if (error) {
                tcp.error_code = error;
                if (onRequestFail)
                    onRequestFail(tcp.error_code.value(), tcp.error_code.message());
                return;
            }
            // Check that response is OK.
            if (onRequestProgress) onRequestProgress(bytes_sent, bytes_recvd);
            std::istream response_stream(&response_buffer);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                if (onResponseFail) onResponseFail(-1);
                return;
            }
            if (status_code != 200) {
                if (onResponseFail) onResponseFail(status_code);
                return;
            }

            // Read the response headers, which are terminated by a blank line.
            asio::async_read_until(
                tcp.socket, response_buffer, "\r\n\r\n",
                std::bind(&HttpClient::read_headers, this, asio::placeholders::error));
        }

        void read_headers(const std::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (onRequestFail)
                    onRequestFail(tcp.error_code.value(), tcp.error_code.message());
                return;
            }
            // Process the response headers.
            response.clear();
            std::istream response_stream(&response_buffer);
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                response.appendHeader(header);

            // Write whatever content we already have to output.
            std::ostringstream content_buffer;
            if (response_buffer.size() > 0) {
                content_buffer << &response_buffer;
                response.setContent(content_buffer.str());
            }

            // Start reading remaining data until EOF.
            asio::async_read(
                tcp.socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&HttpClient::read_content, this, asio::placeholders::error));
        }

        void read_content(const std::error_code &error) {
            if (error) {
                if (onRequestComplete) onRequestComplete(response);
                return;
            }
            // Write all of the data that has been read so far.
            std::ostringstream stream_buffer;
            stream_buffer << &response_buffer;
            if (!stream_buffer.str().empty()) response.setContent(stream_buffer.str());

            // Continue reading remaining data until EOF.
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
            tcp.context.stop();
            pool->stop();
            clearRequest();
            clearPayload();
            clearResponse();
        }

        /*HTTP SETTINGS*/
        void setHost(const std::string &url = "localhost",
                     const std::string &port = "") {
            host = url;
            service = port;
        }

        std::string getHost() const { return host; }
        std::string getPort() const { return service; }

        void setTimeout(uint8_t value = 3) { timeout = value; }
        uint8_t getTimeout() const { return timeout; }

        void setMaxAttemp(uint8_t value = 3) { maxAttemp = value; }
        uint8_t getMaxAttemp() const { return timeout; }

        /*REQUEST DATA*/
        void setRequest(const FRequest &value) { request = value; }
        FRequest getRequest() const { return request; }

        void setRequestMethod(EVerb requestMethod = EVerb::GET) {
            request.verb = requestMethod;
        }

        EVerb getRequestMethod() const { return request.verb; }

        void setVersion(const std::string &version = "2.0") {
            request.version = version;
        }

        std::string getVersion() const { return request.version; }

        void setPath(const std::string &path = "/") {
            request.path = path.empty() ? "/" : path;
        }

        std::string getPath() const { return request.path; }

        void appendParams(const std::string &key, const std::string &value) {
            request.params[key] = value;
        }

        void clearParams() { request.params.clear(); }
        void removeParam(const std::string &key) { request.params.erase(key); }

        bool hasParam(const std::string &key) const {
            return request.params.find(key) != request.params.end();
        }

        std::map<std::string, std::string> getParams() const {
            return request.params;
        }

        void appendHeaders(const std::string &key, const std::string &value) {
            request.headers[key] = value;
        }

        void clearHeaders() { request.headers.clear(); }
        void removeHeader(const std::string &key) { request.headers.erase(key); }

        bool hasHeader(const std::string &key) const {
            return request.headers.find(key) != request.headers.end();
        }

        std::map<std::string, std::string> GetHeaders() const {
            return request.headers;
        }

        void setBody(const std::string &value) { request.body = value; }
        void clearBody() { request.body.clear(); }
        std::string getBody() const { return request.body; }

        FRequest getRequestData() const { return request; }

        /*SECURITY LAYER*/
        bool load_private_key_data(const std::string &key_data) {
            try {
                const asio::const_buffer buffer(key_data.data(), key_data.size());
                tcp.ssl_context.use_private_key(buffer, asio::ssl::context::pem);
            } catch (const std::exception &e) {
                if (onError) onError(-1, e.what());
                return false;
            }
            return true;
        }

        bool load_private_key_file(const std::string &filename) {
            try {
                tcp.ssl_context.use_private_key_file(filename, asio::ssl::context::pem);
            } catch (const std::exception &e) {
                if (onError) onError(-1, e.what());
                return false;
            }
            return true;
        }

        bool load_certificate_data(const std::string &cert_data) {
            try {
                const asio::const_buffer buffer(cert_data.data(), cert_data.size());
                tcp.ssl_context.use_certificate(buffer, asio::ssl::context::pem);
            } catch (const std::exception &e) {
                if (onError) onError(-1, e.what());
                return false;
            }
            return true;
        }

        bool load_certificate_file(const std::string &filename) {
            try {
                tcp.ssl_context.use_certificate_file(filename, asio::ssl::context::pem);
            } catch (const std::exception &e) {
                if (onError) onError(-1, e.what());
                return false;
            }
            return true;
        }

        bool load_certificate_chain_data(const std::string &cert_chain_data) {
            try {
                const asio::const_buffer buffer(cert_chain_data.data(),
                                                cert_chain_data.size());
                tcp.ssl_context.use_certificate_chain(buffer);
            } catch (const std::exception &e) {
                if (onError) onError(-1, e.what());
                return false;
            }
            return true;
        }

        bool load_certificate_chain_file(const std::string &filename) {
            try {
                tcp.ssl_context.use_certificate_chain_file(filename);
            } catch (const std::exception &e) {
                if (onError) onError(-1, e.what());
                return false;
            }
            return true;
        }

        bool load_verify_file(const std::string &filename) {
            try {
                tcp.ssl_context.load_verify_file(filename);
            } catch (const std::exception &e) {
                if (onError) onError(-1, e.what());
                return false;
            }
            return true;
        }

        /*PAYLOAD*/
        void preparePayload() {
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

        void async_preparePayload() {
            asio::post(*pool, [this]() {
                mutexPayload.lock();
                preparePayload();
                if (onAsyncPayloadFinished) onAsyncPayloadFinished();
                mutexPayload.unlock();
            });
        }

        std::string getPayloadData() const { return payload; }

        /*RESPONSE DATA*/
        FResponse getResponseData() const { return response; }

        /*CONNECTION*/
        bool processRequest() {
            if (!pool && !payload.empty()) return false;

            asio::post(*pool, std::bind(&HttpClientSsl::run_context_thread, this));
            return true;
        }

        void cancelRequest() {
            tcp.context.stop();
            tcp.ssl_socket.async_shutdown([&](const std::error_code &error) {
                if (error) {
                    tcp.error_code = error;
                    if (onError) onError(error.value(), error.message());
                    tcp.error_code.clear();
                }
                tcp.ssl_socket.lowest_layer().shutdown(
                    asio::ip::tcp::socket::shutdown_both, tcp.error_code);
                if (tcp.error_code && onError)
                    onError(tcp.error_code.value(), tcp.error_code.message());
                tcp.ssl_socket.lowest_layer().close(tcp.error_code);
                if (tcp.error_code && onError)
                    onError(tcp.error_code.value(), tcp.error_code.message());
                if (onRequestCanceled) onRequestCanceled();
            });
        }

        /*MEMORY MANAGER*/
        void clearRequest() { request.clear(); }
        void clearPayload() { payload.clear(); }
        void clearResponse() { response.clear(); }

        /*ERRORS*/
        int getErrorCode() const { return tcp.error_code.value(); }
        std::string getErrorMessage() const { return tcp.error_code.message(); }

        /*EVENTS*/
        std::function<void()> onAsyncPayloadFinished;
        std::function<void(const FResponse)> onRequestComplete;
        std::function<void()> onRequestCanceled;
        std::function<void(const size_t, const size_t)> onRequestProgress;
        std::function<void(const int)> onRequestWillRetry;
        std::function<void(const int, const std::string &)> onRequestFail;
        std::function<void(const int)> onResponseFail;
        std::function<void(const int, const std::string &)> onError;

    private:
        std::unique_ptr<asio::thread_pool> pool =
                std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutexPayload;
        std::mutex mutexIO;
        std::string host = "localhost";
        std::string service;
        uint8_t timeout = 3;
        uint8_t maxAttemp = 3;
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
            mutexIO.lock();
            while (tcp.attemps_fail <= maxAttemp) {
                if (onRequestWillRetry && tcp.attemps_fail > 0)
                    onRequestWillRetry(tcp.attemps_fail);
                tcp.error_code.clear();
                tcp.context.restart();
                tcp.resolver.async_resolve(
                    getHost(), getPort(),
                    std::bind(&HttpClientSsl::resolve, this, asio::placeholders::error,
                              asio::placeholders::results));
                tcp.context.run();
                if (!tcp.error_code) break;
                tcp.attemps_fail++;
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
            }
            consume_stream_buffers();
            mutexIO.unlock();
        }

        void consume_stream_buffers() {
            request_buffer.consume(request_buffer.size());
            response_buffer.consume(request_buffer.size());
        }

        void resolve(const std::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                tcp.error_code = error;
                if (onRequestFail)
                    onRequestFail(tcp.error_code.value(), tcp.error_code.message());
                return;
            }
            // Attempt a connection to each endpoint in the list until we
            // successfully establish a connection.
            tcp.endpoints = endpoints;
            asio::async_connect(
                tcp.ssl_socket.lowest_layer(), tcp.endpoints,
                std::bind(&HttpClientSsl::connect, this, asio::placeholders::error));
        }

        void connect(const std::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (onRequestFail)
                    onRequestFail(tcp.error_code.value(), tcp.error_code.message());
                return;
            }
            // The connection was successful. Send the request.
            tcp.ssl_socket.async_handshake(asio::ssl::stream_base::client,
                                           std::bind(&HttpClientSsl::ssl_handshake,
                                                     this, asio::placeholders::error));
        }

        void ssl_handshake(const std::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (onRequestFail)
                    onRequestFail(tcp.error_code.value(), tcp.error_code.message());
                return;
            }
            std::ostream request_stream(&request_buffer);
            request_stream << payload;
            asio::async_write(tcp.ssl_socket, request_buffer,
                              std::bind(&HttpClientSsl::write_request, this,
                                        asio::placeholders::error,
                                        asio::placeholders::bytes_transferred));
        }

        void write_request(const std::error_code &error, const size_t bytes_sent) {
            if (error) {
                tcp.error_code = error;
                if (onRequestFail)
                    onRequestFail(tcp.error_code.value(), tcp.error_code.message());
                return;
            }
            // Read the response status line. The response_ streambuf will
            // automatically grow to accommodate the entire line. The growth may be
            // limited by passing a maximum size to the streambuf constructor.
            if (onRequestProgress) onRequestProgress(bytes_sent, 0);
            asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n",
                                   std::bind(&HttpClientSsl::read_status_line, this,
                                             asio::placeholders::error, bytes_sent,
                                             asio::placeholders::bytes_transferred));
        }

        void read_status_line(const std::error_code &error, const size_t bytes_sent,
                              const size_t bytes_recvd) {
            if (error) {
                tcp.error_code = error;
                if (onRequestFail)
                    onRequestFail(tcp.error_code.value(), tcp.error_code.message());
                return;
            }
            // Check that response is OK.
            if (onRequestProgress) onRequestProgress(bytes_sent, bytes_recvd);
            std::istream response_stream(&response_buffer);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                if (onResponseFail) onResponseFail(-1);
                return;
            }
            if (status_code != 200) {
                if (onResponseFail) onResponseFail(status_code);
                return;
            }

            // Read the response headers, which are terminated by a blank line.
            asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n\r\n",
                                   std::bind(&HttpClientSsl::read_headers, this,
                                             asio::placeholders::error));
        }

        void read_headers(const std::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (onRequestFail)
                    onRequestFail(tcp.error_code.value(), tcp.error_code.message());
                return;
            }
            // Process the response headers.
            response.clear();
            std::istream response_stream(&response_buffer);
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                response.appendHeader(header);

            // Write whatever content we already have to output.
            std::ostringstream content_buffer;
            if (response_buffer.size() > 0) {
                content_buffer << &response_buffer;
                response.setContent(content_buffer.str());
            }

            // Start reading remaining data until EOF.
            asio::async_read(tcp.ssl_socket, response_buffer,
                             asio::transfer_at_least(1),
                             std::bind(&HttpClientSsl::read_content, this,
                                       asio::placeholders::error));
        }

        void read_content(const std::error_code &error) {
            if (error) {
                if (onRequestComplete) onRequestComplete(response);
                return;
            }
            // Write all of the data that has been read so far.
            std::ostringstream stream_buffer;
            stream_buffer << &response_buffer;
            if (!stream_buffer.str().empty()) response.setContent(stream_buffer.str());

            // Continue reading remaining data until EOF.
            asio::async_read(tcp.ssl_socket, response_buffer,
                             asio::transfer_at_least(1),
                             std::bind(&HttpClientSsl::read_content, this,
                                       asio::placeholders::error));
        }
    };
#endif
} // namespace InternetProtocol
