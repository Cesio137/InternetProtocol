#pragma once

#include <Net/Message.h>

namespace InternetProtocol {
    class TCPClient {
    public:
        TCPClient() {
        }

        ~TCPClient() {
        }

        /*HOST*/
        void setHost(const std::string &ip, const std::string &port) {
            host = ip;
            service = port;
        }

        std::string getHost() const { return host; }
        std::string getPort() const { return service; }

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
        void send(const std::string &message) {
            if (!pool && !isConnected() && !message.empty())
                return;

            asio::post(*pool, std::bind(&TCPClient::package_string, this, message));
        }

        void send_buffer(const std::vector<std::byte> &buffer) {
            if (!pool && !isConnected() && !buffer.empty())
                return;

            asio::post(*pool, std::bind(&TCPClient::package_buffer, this, buffer));
        }

        void async_read() {
            if (!isConnected())
                return;

            asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
                             std::bind(&TCPClient::read, this, asio::placeholders::error,
                                       asio::placeholders::bytes_transferred)
            );
        }

        /*CONNECTION*/
        void connect() {
            if (!pool)
                return;

            asio::post(*pool, std::bind(&TCPClient::run_context_thread, this));
        }

        bool isConnected() const { return tcp.socket.is_open(); }

        void close() {
            tcp.context.stop();
            tcp.socket.shutdown(asio::ip::tcp::socket::shutdown_both, tcp.error_code);
            if (tcp.error_code && onError) onError(tcp.error_code.value(), tcp.error_code.message());

            tcp.socket.close(tcp.error_code);
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
            response_buffer.consume(asio::buffer_size(response_buffer.data()));
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
            rbuffer.size = asio::buffer_size(response_buffer.data());
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
