#pragma once

#include <Net/Message.h>

namespace InternetProtocol {
    class UDPClient {
    public:
        UDPClient() {
        }

        ~UDPClient() {
            udp.context.stop();
            consume_receive_buffer();
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
        void setMaxSendBufferSize(int value = 1024) { maxSendBufferSize = value; }
        int getMaxSendBufferSize() const { return maxSendBufferSize; }
        void setMaxReceiveBufferSize(int value = 1024) { maxReceiveBufferSize = value; }
        int getMaxReceiveBufferSize() const { return maxReceiveBufferSize; }
        void setSplitPackage(bool value = true) { splitBuffer = value; }
        bool getSplitPackage() const { return splitBuffer; }

        /*MESSAGE*/
        void send(const std::string &message) {
            if (!pool && !isConnected() && !message.empty())
                return;

            asio::post(*pool, std::bind(&UDPClient::package_string, this, message));
        }

        void sendRaw(const std::vector<std::byte> &buffer) {
            if (!pool && !isConnected() && !buffer.empty())
                return;

            asio::post(*pool, std::bind(&UDPClient::package_buffer, this, buffer));
        }

        void async_read() {
            if (!isConnected())
                return;

            udp.socket.async_receive_from(asio::buffer(rbuffer.rawData, rbuffer.rawData.size()), udp.endpoints,
                                          std::bind(&UDPClient::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));
        }

        /*CONNECTION*/
        void connect() {
            if (!pool)
                return;

            asio::post(*pool, std::bind(&UDPClient::run_context_thread, this));
        }

        bool isConnected() const { return udp.socket.is_open(); }

        void close() {
            udp.context.stop();
            udp.socket.close(udp.error_code);
            if (udp.error_code && onError)
                onError(udp.error_code.value(), udp.error_code.message());
            if (onClose)
                onClose();
        }

        /*ERRORS*/
        int getErrorCode() const { return udp.error_code.value(); }
        std::string getErrorMessage() const { return udp.error_code.message(); }

        /*EVENTS*/
        std::function<void()> onConnected;
        std::function<void()> onClose;
        std::function<void(int)> onConnectionRetry;
        std::function<void(const size_t)> onMessageSent;
        std::function<void(const size_t, const FUdpMessage)> onMessageReceived;
        std::function<void(const int, const std::string &)> onError;

    private:
        std::unique_ptr<asio::thread_pool> pool = std::make_unique<asio::thread_pool>(
            std::thread::hardware_concurrency());
        std::mutex mutexIO;
        std::mutex mutexBuffer;
        FAsioUdp udp;
        std::string host = "localhost";
        std::string service;
        uint8_t timeout = 3;
        uint8_t maxAttemp = 3;
        bool splitBuffer = true;
        int maxSendBufferSize = 1024;
        int maxReceiveBufferSize = 1024;
        FUdpMessage rbuffer;

        void package_string(const std::string &str) {
            mutexBuffer.lock();
            if (!splitBuffer || str.size() <= maxSendBufferSize) {
                udp.socket.async_send_to(asio::buffer(str.data(), str.size()), udp.endpoints,
                                         std::bind(&UDPClient::send_to, this, asio::placeholders::error,
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
                udp.socket.async_send_to(asio::buffer(strshrink.data(), strshrink.size()), udp.endpoints,
                                         std::bind(&UDPClient::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                string_offset += package_size;
            }
            mutexBuffer.unlock();
        }

        void package_buffer(const std::vector<std::byte> &buffer) {
            mutexBuffer.lock();
            if (!splitBuffer || buffer.size() <= maxSendBufferSize) {
                udp.socket.async_send_to(asio::buffer(buffer.data(), buffer.size()), udp.endpoints,
                                         std::bind(&UDPClient::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                mutexBuffer.unlock();
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = maxSendBufferSize;
            while (buffer_offset < buffer.size()) {
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                std::vector<std::byte> sbuffer(buffer.begin() + buffer_offset,
                                               buffer.begin() + buffer_offset + package_size);
                udp.socket.async_send_to(asio::buffer(sbuffer.data(), sbuffer.size()), udp.endpoints,
                                         std::bind(&UDPClient::send_to, this, asio::placeholders::error,
                                                   asio::placeholders::bytes_transferred));
                buffer_offset += package_size;
            }
            mutexBuffer.unlock();
        }

        void consume_receive_buffer() {
            rbuffer.rawData.clear();
            rbuffer.rawData.resize(maxReceiveBufferSize);
        }

        /*ASYNC HANDLER FUNCTIONS*/
        void run_context_thread() {
            mutexIO.lock();
            while (udp.attemps_fail <= maxAttemp) {
                if (onConnectionRetry && udp.attemps_fail > 0)
                    onConnectionRetry(udp.attemps_fail);
                udp.error_code.clear();
                udp.context.restart();
                udp.resolver.async_resolve(asio::ip::udp::v4(), host, service,
                                           std::bind(&UDPClient::resolve, this, asio::placeholders::error,
                                                     asio::placeholders::results));
                udp.context.run();
                if (!udp.error_code) break;
                udp.attemps_fail++;
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
            }
            mutexIO.unlock();
        }

        void resolve(const std::error_code &error, asio::ip::udp::resolver::results_type results) {
            if (error) {
                udp.error_code = error;
                if (onError) onError(error.value(), error.message());
                return;
            }
            udp.endpoints = *results;
            udp.socket.async_connect(udp.endpoints,
                                     std::bind(&UDPClient::conn, this, asio::placeholders::error));
        }

        void conn(const std::error_code &error) {
            if (error) {
                udp.error_code = error;
                if (onError) onError(error.value(), error.message());
                return;
            }
            consume_receive_buffer();
            udp.socket.async_receive_from(asio::buffer(rbuffer.rawData, rbuffer.rawData.size()), udp.endpoints,
                                          std::bind(&UDPClient::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));

            if (onConnected)
                onConnected();
        }

        void send_to(const std::error_code &error, const size_t bytes_sent) {
            if (error) {
                udp.error_code = error;
                if (onError) onError(error.value(), error.message());
                return;
            }

            if (onMessageSent)
                onMessageSent(bytes_sent);
        }

        void receive_from(const std::error_code &error, const size_t bytes_recvd) {
            if (error) {
                udp.error_code = error;
                if (onError) onError(error.value(), error.message());
                return;
            }

            udp.attemps_fail = 0;
            rbuffer.size = bytes_recvd;
            rbuffer.rawData.resize(bytes_recvd);
            if (onMessageReceived)
                onMessageReceived(bytes_recvd, rbuffer);

            consume_receive_buffer();
            udp.socket.async_receive_from(asio::buffer(rbuffer.rawData, rbuffer.rawData.size()), udp.endpoints,
                                          std::bind(&UDPClient::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));
        }
    };
}
