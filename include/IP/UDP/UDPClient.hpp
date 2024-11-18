#pragma once

#include <IP/Net/Message.hpp>

namespace InternetProtocol {
    class UDPClient {
    public:
        UDPClient() {
        }

        ~UDPClient() {
            pool->wait();
            if (isConnected()) close();
            rbuffer.raw_data.clear();
            rbuffer.raw_data.resize(0);
        }

        /*HOST | LOCAL*/
        void setHost(const std::string &ip, const std::string &port) {
            host = ip;
            service = port;
        }

        std::string getLocalAdress() const {
            if (isConnected())
                return udp.socket.local_endpoint().address().to_string();
            return "";
        }

        std::string getLocalPort() const {
            if (isConnected())
                return std::to_string(udp.socket.local_endpoint().port());
            return "";
        }

        std::string getRemoteAdress() const {
            if (isConnected())
                return udp.socket.remote_endpoint().address().to_string();
            return host;
        }

        std::string getRemotePort() const {
            if (isConnected())
                return std::to_string(udp.socket.remote_endpoint().port());
            return service;
        }

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
        bool send(const std::string &message) {
            if (!pool && !isConnected() && !message.empty())
                return false;

            asio::post(*pool, std::bind(&UDPClient::package_string, this, message));
            return true;
        }

        bool sendRaw(const std::vector<std::byte> &buffer) {
            if (!pool && !isConnected() && !buffer.empty())
                return false;

            asio::post(*pool, std::bind(&UDPClient::package_buffer, this, buffer));
            return true;
        }

        bool async_read() {
            if (!isConnected())
                return false;

            udp.socket.async_receive_from(asio::buffer(rbuffer.raw_data, rbuffer.raw_data.size()), udp.endpoints,
                                          std::bind(&UDPClient::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));
            return true;
        }

        /*CONNECTION*/
        bool connect() {
            if (!pool && isConnected())
                return false;

            asio::post(*pool, std::bind(&UDPClient::run_context_thread, this));
            return true;
        }

        bool isConnected() const { return udp.socket.is_open(); }

        void close() {
            asio::error_code ec;
            udp.context.stop();
            udp.socket.shutdown(asio::ip::udp::socket::shutdown_both, ec);
            if (ec && onError) {
                onError(ec);
                return;
            }
            udp.socket.close(ec);
            if (ec && onError) {
                onError(ec);
                return;
            }
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
        std::function<void(const asio::error_code &)> onError;

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
            rbuffer.raw_data.clear();
            rbuffer.raw_data.resize(maxReceiveBufferSize);
        }

        /*ASYNC HANDLER FUNCTIONS*/
        void run_context_thread() {
            mutexIO.lock();
            udp.resolver.async_resolve(asio::ip::udp::v4(), host, service,
                                       std::bind(&UDPClient::resolve, this, asio::placeholders::error,
                                                 asio::placeholders::results));
            while (udp.attemps_fail <= maxAttemp) {
                if (onConnectionRetry && udp.attemps_fail > 0)
                    onConnectionRetry(udp.attemps_fail);
                udp.error_code.clear();
                udp.context.restart();
                udp.context.run();
                if (!udp.error_code)
                    break;
                udp.attemps_fail++;
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
            }
            consume_receive_buffer();
            udp.attemps_fail = 0;
            mutexIO.unlock();
        }

        void resolve(const asio::error_code &error, const asio::ip::udp::resolver::results_type &results) {
            if (error) {
                udp.error_code = error;
                if (onError)
                    onError(error);
                return;
            }
            udp.endpoints = *results.begin();
            udp.socket.async_connect(udp.endpoints,
                                     std::bind(&UDPClient::conn, this, asio::placeholders::error));
        }

        void conn(const asio::error_code &error) {
            if (error) {
                udp.error_code = error;
                if (onError)
                    onError(error);
                return;
            }
            consume_receive_buffer();
            udp.socket.async_receive_from(asio::buffer(rbuffer.raw_data, rbuffer.raw_data.size()), udp.endpoints,
                                          std::bind(&UDPClient::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));

            if (onConnected)
                onConnected();
        }

        void send_to(const asio::error_code &error, const size_t bytes_sent) {
            if (error) {
                udp.error_code = error;
                if (onError)
                    onError(error);
                return;
            }

            if (onMessageSent)
                onMessageSent(bytes_sent);
        }

        void receive_from(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                udp.error_code = error;
                if (onError)
                    onError(error);
                return;
            }

            udp.attemps_fail = 0;
            rbuffer.size = bytes_recvd;
            rbuffer.raw_data.resize(bytes_recvd);
            if (onMessageReceived)
                onMessageReceived(bytes_recvd, rbuffer);

            consume_receive_buffer();
            udp.socket.async_receive_from(asio::buffer(rbuffer.raw_data, rbuffer.raw_data.size()), udp.endpoints,
                                          std::bind(&UDPClient::receive_from, this, asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));
        }
    };
}
