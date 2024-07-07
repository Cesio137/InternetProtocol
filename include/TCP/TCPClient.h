#pragma once

#include <Net/Message.h>

namespace InternetProtocol {
    class TCPClient
    {
    public:
        TCPClient() {}
        ~TCPClient() {}

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
        bool isConnected() const { return tcp.socket.is_open(); }
        void close() {             
            tcp.context.stop(); 
            tcp.socket.close();
        }

        /*EVENTS*/
        std::function<void()> onConnected;
        std::function<void(int, const std::string&)> onConnectionError;
		std::function<void(std::size_t)> onMessageSent;
        std::function<void(int, const std::string&)> onMessageSentError;
        std::function<void(int, const tcpMessage)> onMessageReceived;
        std::function<void(int, const std::string&)> onMessageReceivedError;

    private:
        std::unique_ptr<asio::thread_pool> pool = std::make_unique<asio::thread_pool>(2);
        std::mutex mutexIO;
    	std::mutex mutexBuffer;
        FAsioTcp tcp;
        std::string host = "localhost";
		std::string service;
        bool splitPackage = false;
		tcpMessage rbuffer;

        void runContextThread() {
            mutexIO.lock();
            tcp.context.restart();
            tcp.resolver.async_resolve(host, service, 
                std::bind(&TCPClient::resolve, this, asio::placeholders::error, asio::placeholders::endpoint)
            );
        	tcp.context.run();
            mutexIO.unlock();
        }

        void resolve(const std::error_code& err, const asio::ip::tcp::resolver::results_type& endpoints)
		{
			if (err)
			{
				tcp.error_code = err;
				if (onConnectionError)
					onConnectionError(tcp.error_code.value(), tcp.error_code.message());
				return;
			}
			// Attempt a connection to each endpoint in the list until we
			// successfully establish a connection.
			tcp.endpoints = endpoints;

			asio::async_connect(tcp.socket, tcp.endpoints,
				std::bind(&TCPClient::conn, this, asio::placeholders::error)
			);
		}

        void conn(const std::error_code& err)
		{
			if (err)
			{
				tcp.error_code = err;
				if (onConnectionError)
					onConnectionError(tcp.error_code.value(), tcp.error_code.message());
				return;
			}
			// The connection was successful;
            tcp.socket.set_option(asio::socket_base::send_buffer_size(1400));
            tcp.socket.set_option(asio::socket_base::receive_buffer_size(rbuffer.message.size()));

            asio::async_read(tcp.socket, asio::buffer(rbuffer.message, rbuffer.message.size()), asio::transfer_at_least(1),
				std::bind(&TCPClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
			);

			if (onConnected)
				onConnected();
		}

    	void package_buffer(const std::vector<char>& buffer) {
	        mutexBuffer.lock();
            if (!splitPackage || buffer.size() <= 1400) {
                asio::async_write(tcp.socket, asio::buffer(buffer.data(), buffer.size()),
                    std::bind(&TCPClient::write, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
                );
                mutexBuffer.unlock();
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = 1400;
            while (buffer_offset < buffer.size()) {
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                std::vector<char> sbuffer = std::vector<char>(buffer.begin() + buffer_offset, buffer.begin() + buffer_offset + package_size);
                if (sbuffer.back() != '\0')
                    sbuffer.push_back('\0');
                asio::async_write(tcp.socket, asio::buffer(sbuffer.data(), sbuffer.size()),
                    std::bind(&TCPClient::write, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
                );
                buffer_offset += package_size;
            }
        	mutexBuffer.unlock();
        }

    	void write(std::error_code error, std::size_t bytes_sent) {
        	if (error) {
        		if (onMessageSentError)
        			onMessageSentError(error.value(), error.message());
        		return;
        	}
        	if (onMessageSent)
        		onMessageSent(bytes_sent);
        }

		void read(std::error_code error, std::size_t bytes_recvd) {
            if (error) {
                if (onMessageReceivedError)
                    onMessageReceivedError(error.value(), error.message());
                    
                return;
            }
            rbuffer.size = bytes_recvd;
            if (onMessageReceived)
                onMessageReceived(bytes_recvd, rbuffer);

            asio::async_read(tcp.socket, asio::buffer(rbuffer.message, rbuffer.message.size()), asio::transfer_at_least(1),
				std::bind(&TCPClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
			);
        }
    };    
} //InternetProtocol