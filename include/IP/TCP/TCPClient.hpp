#pragma once

#include <IP/Net/Message.hpp>

namespace InternetProtocol {
    class TCPClient {
    public:
        TCPClient() {
        }

        ~TCPClient() {
            tcp.context.stop();
            pool->stop();
            consume_response_buffer();
        }

        /*HOST | LOCAL*/
        void setHost(const std::string &ip, const std::string &port) {
            host = ip;
            service = port;
        }

        std::string getLocalAdress() const {
            if (isConnected())
                return tcp.socket.local_endpoint().address().to_string();
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
        
        std::string getRemovePort() const {
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
            if (!pool && !isConnected() && !message.empty())
                return false;

            asio::post(*pool, std::bind(&TCPClient::package_string, this, message));
            return true;
        }

        bool send_buffer(const std::vector<std::byte> &buffer) {
            if (!pool && !isConnected() && !buffer.empty())
                return false;;

            asio::post(*pool, std::bind(&TCPClient::package_buffer, this, buffer));
            return true;
        }

        bool async_read() {
            if (!isConnected())
                return false;

            asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
                             std::bind(&TCPClient::read, this, asio::placeholders::error,
                                       asio::placeholders::bytes_transferred)
            );
            return true;
        }

        /*CONNECTION*/
         bool connect() {
            if (!pool && isConnected())
                return false;

            asio::post(*pool, std::bind(&TCPClient::run_context_thread, this));
            return true;
        }

        bool isConnected() const { return tcp.socket.is_open(); }

        void close() {
            tcp.context.stop();
            tcp.socket.shutdown(asio::ip::tcp::socket::shutdown_both, tcp.error_code);
            if (tcp.error_code && onError) onError(tcp.error_code.value(), tcp.error_code.message());

            tcp.socket.close(tcp.error_code);
            pool->join();
            if (tcp.error_code && onError) onError(tcp.error_code.value(), tcp.error_code.message());
            if (onClose) onClose();
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
        std::function<void(const int, const std::string &)> onError;

    private:
        std::unique_ptr<asio::thread_pool> pool = std::make_unique<asio::thread_pool>(
            std::thread::hardware_concurrency());
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
                asio::async_write(tcp.socket, asio::buffer(str.data(), str.size()),
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
                asio::async_write(tcp.socket, asio::buffer(strshrink.data(), strshrink.size()),
                                  std::bind(&TCPClient::write, this, asio::placeholders::error,
                                            asio::placeholders::bytes_transferred)
                );
                string_offset += package_size;
            }
            mutexBuffer.unlock();
        }

        void package_buffer(const std::vector<std::byte> &buffer) {
            mutexBuffer.lock();
            if (!splitBuffer || buffer.size() <= maxSendBufferSize) {
                asio::async_write(tcp.socket, asio::buffer(buffer.data(), buffer.size()),
                                  std::bind(&TCPClient::write, this, asio::placeholders::error,
                                            asio::placeholders::bytes_transferred)
                );
                mutexBuffer.unlock();
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = maxSendBufferSize;
            while (buffer_offset < buffer.size()) {
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                std::vector<std::byte> sbuffer(buffer.begin() + buffer_offset,
                                               buffer.begin() + buffer_offset + package_size);
                asio::async_write(tcp.socket, asio::buffer(sbuffer.data(), sbuffer.size()),
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
            while (tcp.attemps_fail <= maxAttemp) {
                if (onConnectionRetry && tcp.attemps_fail > 0)
                    onConnectionRetry(tcp.attemps_fail);
                tcp.error_code.clear();
                tcp.context.restart();
                tcp.resolver.async_resolve(host, service,
                                           std::bind(&TCPClient::resolve, this, asio::placeholders::error,
                                                     asio::placeholders::endpoint));
                tcp.context.run();
                if (!tcp.error_code) break;
                tcp.attemps_fail++;
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
            }
            consume_response_buffer();
            tcp.attemps_fail = 0;
            mutexIO.unlock();
        }

        void resolve(const std::error_code &error, const asio::ip::tcp::resolver::results_type &endpoints) {
            if (error) {
                tcp.error_code = error;
                if (onError)
                    onError(tcp.error_code.value(), tcp.error_code.message());
                return;
            }
            // Attempt a connection to each endpoint in the list until we
            // successfully establish a connection.
            tcp.endpoints = endpoints;

            asio::async_connect(tcp.socket, tcp.endpoints,
                                std::bind(&TCPClient::conn, this, asio::placeholders::error)
            );
        }

        void conn(const std::error_code &error) {
            if (error) {
                tcp.error_code = error;
                if (onError)
                    onError(tcp.error_code.value(), tcp.error_code.message());
                return;
            }

            // The connection was successful;
            consume_response_buffer();
            asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
                             std::bind(&TCPClient::read, this, asio::placeholders::error,
                                       asio::placeholders::bytes_transferred)
            );

            if (onConnected)
                onConnected();
        }

        void write(const std::error_code &error, const size_t bytes_sent) {
            if (error) {
                if (onError)
                    onError(error.value(), error.message());
                return;
            }
            if (onMessageSent)
                onMessageSent(bytes_sent);
        }

        void read(const std::error_code &error, const size_t bytes_recvd) {
            if (error) {
                if (onError)
                    onError(error.value(), error.message());
                return;
            }

            FTcpMessage rbuffer;
            rbuffer.size = response_buffer.size();
            rbuffer.raw_data.resize(rbuffer.size);
            asio::buffer_copy(asio::buffer(rbuffer.raw_data, rbuffer.size), response_buffer.data());

            if (onMessageReceived)
                onMessageReceived(bytes_recvd, rbuffer);

            consume_response_buffer();
            asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
                             std::bind(&TCPClient::read, this, asio::placeholders::error,
                                       asio::placeholders::bytes_transferred)
            );
        }
    };
} //InternetProtocol
