#pragma once

#include <Net/Message.h>

namespace InternetProtocol {
    class UDPClient
    {
    public:
        UDPClient() {

        }
        ~UDPClient() {

        }

        /*HOST*/
        void setHost(const std::string& ip, const std::string& port)
        {
            host = ip;
            service = port;
        }
        std::string getHost() const { return host; }
        std::string getPort() const { return service; }

        /*THREADS*/
		void setThreadNumber(int value = 2) { pool = std::make_unique<asio::thread_pool>(value); }

        /*MESSAGE*/
        void setSplitPackage(bool value = true) { splitPackage = value; }
        bool getSplitPackage() const { return splitPackage; }

        void send(const std::string& message)
        {
            if (!pool || !isConnected())
                return;
            
            asio::post(*pool, [this, message]() {
                std::vector<char> buffer(message.begin(), message.end());
                if (buffer.back() != '\0')
                    buffer.push_back('\0');
                package_buffer(buffer);
            });
        }

        void sendRaw(const std::vector<char>& buffer)
        {
            if (!pool || !isConnected())
                return;

            asio::post(*pool, [this, buffer]() {
                package_buffer(buffer);
            });
        }

        /*CONNECTION*/
        void connect() { 
            if (!pool)
				return;

			asio::post(*pool, [this]() {
			    runContextThread();
			});
        }
        bool isConnected() const { return udp.socket.is_open(); }
        void close() {             
            udp.context.stop(); 
            udp.socket.close();
        }

        /*EVENTS*/
        std::function<void()> onConnected;
        std::function<void(int, const std::string&)> onConnectionError;
        std::function<void(std::size_t)> onMessageSent;
        std::function<void(int, const std::string&)> onMessageSentError;
        std::function<void(int, const udpMessage)> onMessageReceived;
        std::function<void(int, const std::string&)> onMessageReceivedError;

    private:
        std::unique_ptr<asio::thread_pool> pool = std::make_unique<asio::thread_pool>(2);
        std::mutex mutexIO;
        std::mutex mutexBuffer;
        FAsioUdp udp;
        std::string host = "localhost";
		std::string service;
        bool splitPackage = true;
        udpMessage rbuffer;

        /*ASYNC HANDLER FUNCTIONS*/
        void runContextThread() {
            mutexIO.lock();
            udp.context.restart();
            udp.resolver.async_resolve(asio::ip::udp::v4(), host, service,
                std::bind(&UDPClient::resolve, this, asio::placeholders::error, asio::placeholders::results)
            );
            udp.context.run();
            mutexIO.unlock();
        }

        void resolve(std::error_code error, asio::ip::udp::resolver::results_type results) {
            if (error) {
                if (onConnectionError)
                    onConnectionError(error.value(), error.message());
                return;
            }
            udp.endpoints = *results;
            udp.socket.async_connect(udp.endpoints,
                std::bind(&UDPClient::conn, this, asio::placeholders::error)
            );
        }

        void conn(std::error_code error) {
            if (error) {
                if (onConnectionError)
                    onConnectionError(error.value(), error.message());
                return;
            }

            udp.socket.set_option(asio::socket_base::send_buffer_size(1024));
            udp.socket.set_option(asio::socket_base::receive_buffer_size(rbuffer.message.size()));

            udp.socket.async_receive_from(asio::buffer(rbuffer.message, rbuffer.message.size()), udp.endpoints,
                std::bind(&UDPClient::receive_from, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
            );

            if (onConnected)
                onConnected();
        }

        void package_buffer(const std::vector<char>& buffer) {
            mutexBuffer.lock();
            if (!splitPackage || buffer.size() <= 1024) {
                udp.socket.async_send_to(asio::buffer(buffer.data(), buffer.size()), udp.endpoints,
                    std::bind(&UDPClient::send_to, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
                );
                mutexBuffer.unlock();
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = 1023;
            while (buffer_offset < buffer.size()) {
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                std::vector<char> sbuffer = std::vector<char>(buffer.begin() + buffer_offset, buffer.begin() + buffer_offset + package_size);
                if (sbuffer.back() != '\0')
                    sbuffer.push_back('\0');
                udp.socket.async_send_to(asio::buffer(sbuffer.data(), sbuffer.size()), udp.endpoints,
                    std::bind(&UDPClient::send_to, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
                );
                buffer_offset += package_size;
            }
            mutexBuffer.unlock();
        }

        void send_to(std::error_code error, std::size_t bytes_sent) {
            if (error) {
                if (onMessageSentError)
                    onMessageSentError(error.value(), error.message());
                return;
            }
            if (onMessageSent)
                onMessageSent(bytes_sent);
        }

        void receive_from(std::error_code error, std::size_t bytes_recvd) {
            if (error) {
                if (onMessageReceivedError)
                    onMessageReceivedError(error.value(), error.message());
                    
                return;
            }
            rbuffer.size = bytes_recvd;
            if (onMessageReceived)
                onMessageReceived(bytes_recvd, rbuffer);

            udp.socket.async_receive_from(asio::buffer(rbuffer.message, rbuffer.message.size()), udp.endpoints,
                std::bind(&UDPClient::receive_from, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
            );            
        }
    };
}