#pragma once

#include <Net/Common.h>

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

        /*THREADS*/
		void setThreadNumber(int value = 2) { pool = std::make_unique<asio::thread_pool>(value); }

        /*MESSAGE*/
        void send(const std::string& message)
        {
            if (!pool)
                return;

            asio::post(*pool, [this, message]() {
                mutexBuffer.lock();
                std::vector<char> data(message.begin(), message.end());
                package_buffer(data);
                mutexBuffer.unlock();
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
        bool isOpen() const { return udp.socket.is_open(); }
        void close() { 
            udp.context.stop(); 
            udp.socket.close();
        }

        /*EVENTS*/
        std::function<void()> onConnected;
        std::function<void(int, const std::string&)> onConnectionError;
        std::function<void(std::size_t)> onMessageSent;
        std::function<void(int, const std::string&)> onMessageSentError;
        std::function<void(int, const std::string&)> onMessageReceived;
        std::function<void(int, const std::string&)> onMessageReceivedError;

    private:
        std::unique_ptr<asio::thread_pool> pool = std::make_unique<asio::thread_pool>(2);
        std::mutex mutexIO;
        std::mutex mutexBuffer;
        FAsioUdp udp;
        std::string host = "localhost";
		std::string service;
        std::array<char, 1024> rbuffer;
        enum { max_length = 1024 };

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
            udp.socket.async_receive_from(asio::buffer(rbuffer, max_length), udp.endpoints,
                std::bind(&UDPClient::receive_from, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
            );
            if (onConnected)
                onConnected();
        }

        void package_buffer(std::vector<char> data) {
            if (data.size() <= 1024) {
                udp.socket.async_send_to(asio::buffer(data, max_length), udp.endpoints,
                    std::bind(&UDPClient::send_to, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
                );
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = 1024;
            while (buffer_offset < data.size()) {
                size_t package_size = std::min(max_size, data.size() - buffer_offset);
                std::vector<char> sbuffer = std::vector<char>(data.begin() + buffer_offset, data.begin() + buffer_offset + package_size);
                udp.socket.async_send_to(asio::buffer(sbuffer, max_length), udp.endpoints,
                    std::bind(&UDPClient::send_to, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
                );
                buffer_offset += package_size;
            }
            
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
            
            if (onMessageReceived)
                onMessageReceived(bytes_recvd, rbuffer.data());
                
        }
    };
}