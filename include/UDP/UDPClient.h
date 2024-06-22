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

        /*CONNECTION*/
        void connect() { 
            asio::ip::udp::resolver::query query(host, service); 
            udp.resolver.async_resolve(query,
                std::bind(&UDPClient::resolve, this, asio::placeholders::error, asio::placeholders::results)
            );
            udp.context.run();
        }
        void close() { udp.context.stop(); }

        /*EVENTS*/
        std::function<void()> onConnection;
        std::function<void()> onConnectionError;

    private:
        std::unique_ptr<asio::thread_pool> pool = std::make_unique<asio::thread_pool>(2);
        FAsioUdp udp;
        std::string host = "localhost";
		std::string service;
        char data[1024];

        void resolve(const asio::error_code& error, asio::ip::udp::resolver::results_type results) { 
            if (error) {
                if (onConnectionError)
                    onConnectionError();
            }

         }

    };
}