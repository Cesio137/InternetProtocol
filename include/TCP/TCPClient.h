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
        void setTimeout(uint8_t value = 4) { timeout = value; }
        uint8_t getTimeout() const { return timeout; }
        void setMaxAttemp(uint8_t value = 3) { maxAttemp = value; }
        uint8_t getMaxAttemp() const { return timeout; }
        void setMaxSendBufferSize(int value = 1400) { maxSendBufferSize = value; }
        int getMaxSendBufferSize() const { return maxSendBufferSize; }
        void setSplitPackage(bool value = true) { splitBuffer = value; }
        bool getSplitPackage() const { return splitBuffer; }

        /*MESSAGE*/
        void send(const std::string &message) {
            if (!pool || !isConnected())
                return;

            asio::post(*pool, [this, message]() {
                std::vector<std::byte> buffer;
                buffer.resize(message.size());
                std::transform(message.begin(), message.end(), buffer.begin(),
                               [](char c) { return std::byte(c); });
                package_buffer(buffer);
            });
        }

        void sendRaw(const std::vector<std::byte> &buffer) {
            if (!pool || !isConnected())
                return;

            asio::post(*pool, [this, buffer]() {
                package_buffer(buffer);
            });
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

            asio::post(*pool, [this]() {
                runContextThread();
            });
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
        uint8_t timeout = 4;
        uint8_t maxAttemp = 3;
        bool splitBuffer = true;
        int maxSendBufferSize = 1400;
        asio::streambuf response_buffer;

        void runContextThread() {
            mutexIO.lock();
            tcp.context.restart();
            tcp.resolver.async_resolve(host, service,
                                       std::bind(&TCPClient::resolve, this, asio::placeholders::error,
                                                 asio::placeholders::endpoint)
            );
            tcp.context.run();
            if (tcp.error_code && maxAttemp > 0 && timeout > 0) {
                uint8_t attemp = 0;
                while (attemp < maxAttemp) {
                    if (onConnectionRetry)
                        onConnectionRetry(attemp + 1);
                    tcp.error_code.clear();
                    tcp.context.restart();
                    asio::steady_timer timer(tcp.context, asio::chrono::seconds(timeout));
                    timer.async_wait([this](const std::error_code &error) {
                        if (error) {
                            if (onError)
                                onError(error.value(), error.message());
                        }
                        tcp.resolver.async_resolve(host, service,
                                                   std::bind(&TCPClient::resolve, this, asio::placeholders::error,
                                                             asio::placeholders::endpoint)
                        );
                    });
                    tcp.context.run();
                    attemp += 1;
                    if (!tcp.error_code)
                        break;
                }
            }
            clearBuffer();
            mutexIO.unlock();
        }

        void clearBuffer() {
            if (response_buffer.size() > 0)
                response_buffer.consume(response_buffer.size());
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
                                            asio::placeholders::bytes_transferred)
                );
                buffer_offset += package_size;
            }
            mutexBuffer.unlock();
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
            rbuffer.rawData.resize(rbuffer.size);
            asio::buffer_copy(asio::buffer(rbuffer.rawData, rbuffer.size), response_buffer.data());

            if (onMessageReceived)
                onMessageReceived(bytes_recvd, rbuffer);

            response_buffer.consume(asio::buffer_size(response_buffer.data()));
            asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
                             std::bind(&TCPClient::read, this, asio::placeholders::error,
                                       asio::placeholders::bytes_transferred)
            );
        }
    };
} //InternetProtocol
