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

    	/*SETTINGS*/
    	void setTimeout(uint8_t value = 4) { timeout = value; }
    	uint8_t getTimeout() const { return timeout; }
    	void setMaxAttemp(uint8_t value = 3) { maxAttemp = value; }
    	uint8_t getMaxAttemp() const { return timeout; }
    	void setMaxBufferSize(int value = 1400) { maxBufferSize = value; }
    	int getMaxBufferSize() const { return maxBufferSize; }
    	void setSplitPackage(bool value = true) { splitBuffer = value; }
    	bool getSplitPackage() const { return splitBuffer; }

    	/*MESSAGE*/
    	void send(const std::string& message)
        {
        	if (!pool || !isConnected())
        		return;

        	asio::post(*pool, [this, message]() {
				std::vector<uint8_t> buffer(message.begin(), message.end());
				if (buffer.back() != '\0')
					buffer.push_back('\0');
				package_buffer(buffer);
			});
        }

		void sendRaw(const std::vector<uint8_t>& buffer)
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
    	std::function<void(int)> onConnectionRetry;
		std::function<void(std::size_t)> onMessageSent;
        std::function<void(int, const FTcpMessage)> onMessageReceived;
    	std::function<void(int, const std::string&)> onError;

    private:
        std::unique_ptr<asio::thread_pool> pool = std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
        std::mutex mutexIO;
    	std::mutex mutexBuffer;
        FAsioTcp tcp;
        std::string host = "localhost";
		std::string service;
    	uint8_t timeout = 4;
    	uint8_t maxAttemp = 3;
    	bool splitBuffer = true;
    	int maxBufferSize = 1400;
		FTcpMessage rbuffer;

        void runContextThread() {
            mutexIO.lock();
            tcp.context.restart();
            tcp.resolver.async_resolve(host, service, 
                std::bind(&TCPClient::resolve, this, asio::placeholders::error, asio::placeholders::endpoint)
            );
        	tcp.context.run();
        	if (tcp.error_code && maxAttemp > 0 && timeout > 0) {
        		uint8_t attemp = 0;
        		while(attemp < maxAttemp) {
        			if (onConnectionRetry)
        				onConnectionRetry(attemp + 1);
        			tcp.error_code.clear();
        			tcp.context.restart();
        			asio::steady_timer timer(tcp.context, asio::chrono::seconds(timeout));
        			timer.async_wait([this](const std::error_code& error) {
						if (error) {
							if (onError)
								onError(error.value(), error.message());
						}
        				tcp.resolver.async_resolve(host, service,
							std::bind(&TCPClient::resolve, this, asio::placeholders::error, asio::placeholders::endpoint)
						);
					});
        			tcp.context.run();
        			attemp += 1;
        			if(!tcp.error_code)
        				break;
        		}
        	}
            mutexIO.unlock();
        }

        void resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
		{
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

        void conn(const std::error_code& error)
		{
			if (error) {
				tcp.error_code = error;
				if (onError)
					onError(tcp.error_code.value(), tcp.error_code.message());
				return;
			}
			// The connection was successful;
            asio::async_read(tcp.socket, asio::buffer(rbuffer.rawData, rbuffer.rawData.size()), asio::transfer_at_least(1),
				std::bind(&TCPClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
			);

			if (onConnected)
				onConnected();
		}

    	void package_buffer(const std::vector<uint8_t>& buffer) {
	        mutexBuffer.lock();
            if (!splitBuffer || buffer.size() <= maxBufferSize) {
                asio::async_write(tcp.socket, asio::buffer(buffer.data(), buffer.size()),
                    std::bind(&TCPClient::write, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
                );
                mutexBuffer.unlock();
                return;
            }

            size_t buffer_offset = 0;
            const size_t max_size = maxBufferSize - 1;
            while (buffer_offset < buffer.size()) {
                size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
                std::vector<uint8_t> sbuffer(buffer.begin() + buffer_offset, buffer.begin() + buffer_offset + package_size);
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
        		if (onError)
        			onError(error.value(), error.message());
        		return;
        	}
        	if (onMessageSent)
        		onMessageSent(bytes_sent);
        }

		void read(std::error_code error, std::size_t bytes_recvd) {
            if (error) {
                if (onError)
                    onError(error.value(), error.message());
                return;
            }
            rbuffer.size = bytes_recvd;
            if (onMessageReceived)
                onMessageReceived(bytes_recvd, rbuffer);

            asio::async_read(tcp.socket, asio::buffer(rbuffer.rawData, rbuffer.rawData.size()), asio::transfer_at_least(1),
				std::bind(&TCPClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
			);
        }
    };    
} //InternetProtocol