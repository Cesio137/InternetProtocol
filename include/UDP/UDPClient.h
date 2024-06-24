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
                std::vector<char> data(message.begin(), message.end());
                package_buffer(data);
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
        void close() { udp.context.stop(); }

        /*EVENTS*/
        std::function<void()> onConnected;
        std::function<void(int, const std::string&)> onConnectionError;
        std::function<void(std::size_t)> onMessageSent;

    private:
        std::unique_ptr<asio::thread_pool> pool = std::make_unique<asio::thread_pool>(2);
        FAsioUdp udp;
        std::string host = "localhost";
		std::string service;
        std::array<char, 1024> rbuffer;

        /*ASYNC HANDLER FUNCTIONS*/
        void runContextThread() {
            udp.resolver.async_resolve(asio::ip::udp::v4(), host, service,
                std::bind(&UDPClient::resolve, this, asio::placeholders::error, asio::placeholders::results)
            );
            udp.context.run();
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
            if (onConnected)
                onConnected();
        }

        void package_buffer(std::vector<char> data) {
            if (data.size() <= 1024) {
                udp.socket.async_send_to(asio::buffer(data), udp.endpoints,
                    std::bind(&UDPClient::send_to, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
                );

                return;
            }
        }

        void send_to(std::error_code error, std::size_t bytes_sent) {
            if (error) {
                return;
            }
            if (onMessageSent)
                onMessageSent(bytes_sent);
        }
    };
}