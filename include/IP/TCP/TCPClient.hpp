#pragma once

#include <IP/Net/Message.hpp>

namespace InternetProtocol {
    class TCPClient {
    public:
        TCPClient() {
        }

        ~TCPClient() {
            pool->wait();
            if (isConnected()) close();
            consume_response_buffer();
        }

        /*HOST | LOCAL*/
        void setHost(const std::string &ip, const std::string &port) {
            host = ip;
            service = port;
        }

        std::string getLocalAdress() const {
            if (isConnected()) return tcp.socket.local_endpoint().address().to_string();
            return "";
        }

        std::string getLocalPort() const {
            if (isConnected())
                return std::to_string(tcp.socket.local_endpoint().port());
            return "";
        }

        std::string getRemoteAdress() const {
            if (isConnected())
                return tcp.socket.remote_endpoint().address().to_string();
            return host;
        }

        std::string getRemotePort() const {
            if (isConnected())
                return std::to_string(tcp.socket.remote_endpoint().port());
            return service;
        }

        /*SETTINGS*/
        void setTimeout(uint8_t value = 3) { timeout = value; }
        uint8_t getTimeout() const { return timeout; }
        void setMaxAttemp(uint8_t value = 3) { maxAttemp = value; }
        uint8_t getMaxAttemp() const { return timeout; }
        void setMaxSendBufferSize(int value = 1400) { maxSendBufferSize = value; }
        int getMaxSendBufferSize() const { return maxSendBufferSize; }
        void setSplitPackage(bool value = true) { splitBuffer = value; }
        bool getSplitPackage() const { return splitBuffer; }

        /*MESSAGE*/
        bool send(const std::string &message) {
            if (!pool && !isConnected() && !message.empty()) return false;

            asio::post(*pool, std::bind(&TCPClient::package_string, this, message));
            return true;
        }

        bool send_buffer(const std::vector<std::byte> &buffer) {
            if (!pool && !isConnected() && !buffer.empty()) return false;;

            asio::post(*pool, std::bind(&TCPClient::package_buffer, this, buffer));
            return true;
        }

        bool async_read() {
            if (!isConnected()) return false;

            asio::async_read(
                tcp.socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPClient::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
            return true;
        }

        /*CONNECTION*/
        bool connect() {
            if (!pool && isConnected()) return false;

            asio::post(*pool, std::bind(&TCPClient::run_context_thread, this));
            return true;
        }

        bool isConnected() const { return tcp.socket.is_open(); }

        void close() {
            asio::error_code ec;
            tcp.context.stop();
            tcp.socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            if (ec && onError)
                onError(ec);
            tcp.socket.close(tcp.error_code);
            if (ec && onError)
                onError(ec);
        }

        /*ERRORS*/
        int getErrorCode() const { return tcp.error_code.value(); }
        std::string getErrorMessage() const { return tcp.error_code.message(); }

        /*EVENTS*/
        std::function<void()> onConnected;
        std::function<void()> onClose;
        std::function<void(const int)> onConnectionRetry;
        std::function<void(const size_t)> onMessageSent;
        std::function<void(const size_t, const FTcpMessage)> onMessageReceived;
        std::function<void(const asio::error_code &)> onError;

    private:
        std::unique_ptr<asio::thread_pool> pool =
                std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutexIO;
        std::mutex mutexBuffer;
        FAsioTcp tcp;
        std::string host = "localhost";
        std::string service;
        uint8_t timeout = 3;
        uint8_t maxAttemp = 3;
        bool splitBuffer = true;
        int maxSendBufferSize = 1400;
        asio::streambuf response_buffer;

        void package_string(const std::string &str) {
            mutexBuffer.lock();
            if (!splitBuffer || str.size() <= maxSendBufferSize) {
                asio::async_write(
                    tcp.socket, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPClient::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                mutexBuffer.unlock();
                return;
            }

            size_t string_offset = 0;
            const size_t max_size = maxSendBufferSize;
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
            mutexBuffer.unlock();
        }

        void package_buffer(const std::vector<std::byte> &buffer) {
            mutexBuffer.lock();
            if (!splitBuffer || buffer.size() <= maxSendBufferSize) {
                asio::async_write(
                    tcp.socket, asio::buffer(buffer.data(), buffer.size()),
                    std::bind(&TCPClient::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                mutexBuffer.unlock();
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = maxSendBufferSize;
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
            mutexBuffer.unlock();
        }

        void consume_response_buffer() {
            response_buffer.consume(response_buffer.size());
        }

        void run_context_thread() {
            mutexIO.lock();
            tcp.resolver.async_resolve(
                host, service,
                std::bind(&TCPClient::resolve, this, asio::placeholders::error,
                          asio::placeholders::endpoint));
            while (tcp.attemps_fail <= maxAttemp) {
                if (onConnectionRetry && tcp.attemps_fail > 0)
                    onConnectionRetry(tcp.attemps_fail);
                tcp.error_code.clear();
                tcp.context.restart();
                tcp.context.run();
                if (!tcp.error_code) break;
                tcp.attemps_fail++;
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
            }
            consume_response_buffer();
            tcp.attemps_fail = 0;
            mutexIO.unlock();
        }

        void resolve(const asio::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                tcp.error_code = error;
                if (onError) onError(tcp.error_code);
                return;
            }
            // Attempt a connection to each endpoint in the list until we
            // successfully establish a connection.
            tcp.endpoints = endpoints;

            asio::async_connect(
                tcp.socket, tcp.endpoints,
                std::bind(&TCPClient::conn, this, asio::placeholders::error));
        }

        void conn(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (onError) onError(error);
                return;
            }

            // The connection was successful;
            consume_response_buffer();
            asio::async_read(
                tcp.socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPClient::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));

            if (onConnected) onConnected();
        }

        void write(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                tcp.error_code = error;
                if (onError) onError(error);
                return;
            }
            if (onMessageSent) onMessageSent(bytes_sent);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                tcp.error_code = error;
                if (onError) onError(error);
                return;
            }

            FTcpMessage rbuffer;
            rbuffer.size = response_buffer.size();
            rbuffer.raw_data.resize(rbuffer.size);
            asio::buffer_copy(asio::buffer(rbuffer.raw_data, rbuffer.size),
                              response_buffer.data());

            if (onMessageReceived) onMessageReceived(bytes_recvd, rbuffer);

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
        TCPClientSsl() {
        }

        ~TCPClientSsl() {
            pool->wait();
            if (isConnected()) close();
            consume_response_buffer();
        }

        /*HOST | LOCAL*/
        void setHost(const std::string &ip, const std::string &port) {
            host = ip;
            service = port;
        }

        std::string getLocalAdress() const {
            if (isConnected())
                return tcp.ssl_socket.lowest_layer()
                        .local_endpoint()
                        .address()
                        .to_string();
            return "";
        }

        std::string getLocalPort() const {
            if (isConnected())
                return std::to_string(
                    tcp.ssl_socket.lowest_layer().local_endpoint().port());
            return "";
        }

        std::string getRemoteAdress() const {
            if (isConnected())
                return tcp.ssl_socket.lowest_layer()
                        .remote_endpoint()
                        .address()
                        .to_string();
            return host;
        }

        std::string getRemotePort() const {
            if (isConnected())
                return std::to_string(
                    tcp.ssl_socket.lowest_layer().remote_endpoint().port());
            return service;
        }

        /*SETTINGS*/
        void setTimeout(uint8_t value = 3) { timeout = value; }
        uint8_t getTimeout() const { return timeout; }
        void setMaxAttemp(uint8_t value = 3) { maxAttemp = value; }
        uint8_t getMaxAttemp() const { return timeout; }
        void setMaxSendBufferSize(int value = 1400) { maxSendBufferSize = value; }
        int getMaxSendBufferSize() const { return maxSendBufferSize; }
        void setSplitPackage(bool value = true) { splitBuffer = value; }
        bool getSplitPackage() const { return splitBuffer; }

        /*SECURITY LAYER*/
        bool load_private_key_data(const std::string &key_data) {
            if (key_data.empty()) return false;
            asio::error_code ec;
            const asio::const_buffer buffer(key_data.data(), key_data.size());
            tcp.ssl_context.use_private_key(buffer, asio::ssl::context::pem, ec);
            if (ec) {
                if (onError) onError(ec);
                return false;
            }
            return true;
        }

        bool load_private_key_file(const std::string &filename) {
            if (filename.empty()) return false;
            asio::error_code ec;
            tcp.ssl_context.use_private_key_file(filename, asio::ssl::context::pem, ec);
            if (ec) {
                if (onError) onError(ec);
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
                if (onError) onError(ec);
                return false;
            }
            return true;
        }

        bool load_certificate_file(const std::string &filename) {
            if (filename.empty()) return false;
            asio::error_code ec;
            tcp.ssl_context.use_certificate_file(filename, asio::ssl::context::pem, ec);
            if (ec) {
                if (onError) onError(ec);
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
                if (onError) onError(ec);
                return false;
            }
            return true;
        }

        bool load_certificate_chain_file(const std::string &filename) {
            if (filename.empty()) return false;
            asio::error_code ec;
            tcp.ssl_context.use_certificate_chain_file(filename, ec);
            if (ec) {
                if (onError) onError(ec);
                return false;
            }
            return true;
        }

        bool load_verify_file(const std::string &filename) {
            if (filename.empty()) return false;
            asio::error_code ec;
            tcp.ssl_context.load_verify_file(filename, ec);
            if (ec) {
                if (onError) onError(ec);
                return false;
            }
            return true;
        }

        /*MESSAGE*/
        bool send(const std::string &message) {
            if (!pool && !isConnected() && !message.empty()) return false;

            asio::post(*pool, std::bind(&TCPClientSsl::package_string, this, message));
            return true;
        }

        bool send_buffer(const std::vector<std::byte> &buffer) {
            if (!pool && !isConnected() && !buffer.empty()) return false;;

            asio::post(*pool, std::bind(&TCPClientSsl::package_buffer, this, buffer));
            return true;
        }

        bool async_read() {
            if (!isConnected()) return false;

            asio::async_read(
                tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPClientSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
            return true;
        }

        /*CONNECTION*/
        bool connect() {
            if (!pool && isConnected()) return false;

            asio::post(*pool, std::bind(&TCPClientSsl::run_context_thread, this));
            return true;
        }

        bool isConnected() const { return tcp.ssl_socket.lowest_layer().is_open(); }

        void close() {
            asio::error_code ec;
            tcp.context.stop();
            tcp.ssl_socket.async_shutdown([&](const asio::error_code &error) {
                if (error) {
                    if (onError) onError(error);
                    tcp.error_code.clear();
                }
                tcp.context.stop();
                tcp.ssl_socket.lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
                if (ec && onError)
                    onError(ec);
                tcp.ssl_socket.lowest_layer().close(ec);
                if (ec && onError)
                    onError(ec);
                pool->join();
                if (onClose) onClose();
            });
        }

        /*ERRORS*/
        int getErrorCode() const { return tcp.error_code.value(); }
        std::string getErrorMessage() const { return tcp.error_code.message(); }

        /*EVENTS*/
        std::function<void()> onConnected;
        std::function<void()> onClose;
        std::function<void(const int)> onConnectionRetry;
        std::function<void(const size_t)> onMessageSent;
        std::function<void(const size_t, const FTcpMessage)> onMessageReceived;
        std::function<void(const asio::error_code &)> onError;

    private:
        std::unique_ptr<asio::thread_pool> pool =
                std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutexIO;
        std::mutex mutexBuffer;
        FAsioTcpSsl tcp;
        std::string host = "localhost";
        std::string service;
        uint8_t timeout = 3;
        uint8_t maxAttemp = 3;
        bool splitBuffer = true;
        int maxSendBufferSize = 1400;
        asio::streambuf response_buffer;

        void package_string(const std::string &str) {
            mutexBuffer.lock();
            if (!splitBuffer || str.size() <= maxSendBufferSize) {
                asio::async_write(
                    tcp.ssl_socket, asio::buffer(str.data(), str.size()),
                    std::bind(&TCPClientSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                mutexBuffer.unlock();
                return;
            }

            size_t string_offset = 0;
            const size_t max_size = maxSendBufferSize;
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
            mutexBuffer.unlock();
        }

        void package_buffer(const std::vector<std::byte> &buffer) {
            mutexBuffer.lock();
            if (!splitBuffer || buffer.size() <= maxSendBufferSize) {
                asio::async_write(
                    tcp.ssl_socket, asio::buffer(buffer.data(), buffer.size()),
                    std::bind(&TCPClientSsl::write, this, asio::placeholders::error,
                              asio::placeholders::bytes_transferred));
                mutexBuffer.unlock();
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = maxSendBufferSize;
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
            mutexBuffer.unlock();
        }

        void consume_response_buffer() {
            response_buffer.consume(response_buffer.size());
        }

        void run_context_thread() {
            mutexIO.lock();
            tcp.resolver.async_resolve(
                host, service,
                std::bind(&TCPClientSsl::resolve, this, asio::placeholders::error,
                          asio::placeholders::endpoint));
            while (tcp.attemps_fail <= maxAttemp) {
                if (onConnectionRetry && tcp.attemps_fail > 0)
                    onConnectionRetry(tcp.attemps_fail);
                tcp.error_code.clear();
                tcp.context.restart();
                tcp.context.run();
                if (!tcp.error_code) break;
                tcp.attemps_fail++;
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
            }
            consume_response_buffer();
            tcp.attemps_fail = 0;
            mutexIO.unlock();
        }

        void resolve(const asio::error_code &error,
                     const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                tcp.error_code = error;
                if (onError) onError(error);
                return;
            }
            // Attempt a connection to each endpoint in the list until we
            // successfully establish a connection.
            tcp.endpoints = endpoints;
            asio::async_connect(
                tcp.ssl_socket.lowest_layer(), tcp.endpoints,
                std::bind(&TCPClientSsl::conn, this, asio::placeholders::error));
        }

        void conn(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (onError) onError(error);
                return;
            }

            // The connection was successful;
            tcp.ssl_socket.async_handshake(asio::ssl::stream_base::client,
                                           std::bind(&TCPClientSsl::ssl_handshake, this,
                                                     asio::placeholders::error));
        }

        void ssl_handshake(const asio::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (onError) onError(error);
                return;
            }
            consume_response_buffer();
            asio::async_read(
                tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPClientSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));

            if (onConnected) onConnected();
        }

        void write(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                tcp.error_code = error;
                if (onError) onError(error);
                return;
            }
            if (onMessageSent) onMessageSent(bytes_sent);
        }

        void read(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                tcp.error_code = error;
                if (onError) onError(error);
                return;
            }

            FTcpMessage rbuffer;
            rbuffer.size = response_buffer.size();
            rbuffer.raw_data.resize(rbuffer.size);
            asio::buffer_copy(asio::buffer(rbuffer.raw_data, rbuffer.size),
                              response_buffer.data());

            if (onMessageReceived) onMessageReceived(bytes_recvd, rbuffer);

            consume_response_buffer();
            asio::async_read(
                tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
                std::bind(&TCPClientSsl::read, this, asio::placeholders::error,
                          asio::placeholders::bytes_transferred));
        }
    };
#endif
} // namespace InternetProtocol
