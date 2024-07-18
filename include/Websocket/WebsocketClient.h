#pragma once

#include "Net/Message.h"


namespace InternetProtocol {
    class WebsocketClient
    {
    public:
        WebsocketClient() {}
        ~WebsocketClient() {}

        /*HOST*/
		void setHost(const std::string& url = "localhost", const std::string& port = "")
		{
			host = url;
			service = port;
		}
		std::string getHost() const { return host; }
		std::string getPort() const { return service; }

        /*SETTINGS*/
		void setTimeout(uint8_t value = 4) { timeout = value; }
		uint8_t getTimeout() const { return timeout; }
		void setMaxAttemp(uint8_t value = 3) { maxAttemp = value; }
		uint8_t getMaxAttemp() const { return timeout; }

        /*HANDSHAKE*/
    	void setPath(const std::string& value = "chat") { handShake.insert_or_assign("Path", value); }
    	std::string getPath() const { return handShake.at("Path"); }
    	void setVersion(const std::string& value = "1.1") { handShake.insert_or_assign("Version", value); }
    	std::string getVersion() const { return handShake.at("Version"); }
        void setKey(const std::string& value = "dGhlIHNhbXBsZSBub25jZQ==") { handShake.insert_or_assign("Sec-WebSocket-Key", value); }
        std::string getKey() const { return handShake.at("Sec-WebSocket-Key"); }
        void setOrigin(const std::string& value = "client") { handShake.insert_or_assign("Origin", value); }
        std::string getOrigin() const { return handShake.at("Origin"); }
        void setSecProtocol(const std::string& value = "chat, superchat") { handShake.insert_or_assign("Sec-WebSocket-Protocol", value); }
        std::string getSecProtocol() const { return handShake.at("Sec-WebSocket-Protocol"); }
        void setSecVersion(const std::string& value = "13") { handShake.insert_or_assign("Sec-WebSocket-Version", value); }
        std::string getSecVersion() const { return handShake.at("Sec-WebSocket-Version"); }

        /*MESSAGE*/
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
    	std::function<void(int)> onConnectionRetry;
    	std::function<void()> onHandshakeReceived;
		std::function<void(std::size_t)> onMessageSent;
        std::function<void(int, const std::string&)> onMessageReceived;
        std::function<void(int, const std::string&)> onError;

    private:
        std::unique_ptr<asio::thread_pool> pool = std::make_unique<asio::thread_pool>(std::thread::hardware_concurrency());
		std::mutex mutexIO;
        std::mutex mutexBuffer;
		std::string host = "localhost";
		std::string service;
		uint8_t timeout = 4;
		uint8_t maxAttemp = 3;
    	bool splitBuffer = true;
    	int maxBufferSize = 1400;
		FAsioTcp tcp;
		asio::streambuf request_buffer;
		asio::streambuf response_buffer;
        std::map<std::string, std::string> handShake = {
	        {"Path", "chat"},
		    {"Version", "1.1"},
			{"Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ=="},
            {"Origin", "client"},
            {"Sec-WebSocket-Protocol", "chat, superchat"}, 
            {"Sec-WebSocket-Version", "13"}
		};

        void runContextThread() {
            mutexIO.lock();
            tcp.context.restart();
            tcp.resolver.async_resolve(host, service, 
                std::bind(&WebsocketClient::resolve, this, asio::placeholders::error, asio::placeholders::endpoint)
            );
            tcp.context.run();
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
				std::bind(&WebsocketClient::conn, this, asio::placeholders::error)
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
            std::stringstream request;
            request << "GET /" + handShake.at("Path") + " HTTP/" + handShake.at("Version") + "\r\n";
            request << "Host: " + host + "\r\n";
            request << "Upgrade: websocket\r\n";
            request << "Connection: Upgrade\r\n";
            request << "Sec-WebSocket-Key: " + handShake.at("Sec-WebSocket-Key") +"\r\n";
            request << "Origin: " + handShake.at("Origin") +"\r\n";
            request << "Sec-WebSocket-Protocol: " + handShake.at("Sec-WebSocket-Protocol") +"\r\n";
            request << "Sec-WebSocket-Version: " + handShake.at("Sec-WebSocket-Version") +"\r\n";
            request << "\r\n";
            std::ostream request_stream(&request_buffer);
			request_stream << request.str();
            asio::async_write(tcp.socket, request_buffer,
				std::bind(&WebsocketClient::write_handshake, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
			);
		}

        void write_handshake(const std::error_code& error, std::size_t bytes_sent)
		{
			if (error) {
				tcp.error_code = error;
				if (onError)
					onError(tcp.error_code.value(), tcp.error_code.message());
				return;
			}
			// Read the response status line. The response_ streambuf will
			// automatically grow to accommodate the entire line. The growth may be
			// limited by passing a maximum size to the streambuf constructor.
			asio::async_read_until(tcp.socket, response_buffer, "\r\n",
				std::bind(&WebsocketClient::read_handshake, this, asio::placeholders::error, bytes_sent, asio::placeholders::bytes_transferred)
			);
		}

        void read_handshake(const std::error_code& error, std::size_t bytes_sent, std::size_t bytes_recvd)
		{
			if (error) {
				tcp.error_code = error;
				if (onError)
					onError(tcp.error_code.value(), tcp.error_code.message());
				return;
			}
			// Check that response is OK.
			std::istream response_stream(&response_buffer);
			std::string http_version;
			response_stream >> http_version;
			unsigned int status_code;
			response_stream >> status_code;
			std::string status_message;
			std::getline(response_stream, status_message);
			if (!response_stream || http_version.substr(0, 5) != "HTTP/")
			{
				if (onError)
					onError(-1, "Invalid response.");
				return;
			}
			if (status_code != 101)
			{
				if (onError)
					onError(status_code, "Invalid status code.");
				return;
			}

			response_buffer.consume(response_buffer.size());

			asio::async_read(tcp.socket, response_buffer,
                std::bind(&WebsocketClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
            );

            // The connection was successful
            if (onConnected)
                onConnected();
		}
		
        void package_buffer(const std::vector<char>& buffer) { 
	        mutexBuffer.lock();
            clientDataFrame dataFrame;
			dataFrame.payload.assign(buffer.begin(), buffer.end());
            asio::async_write(tcp.socket, asio::buffer(dataFrame.encodeFrame().data(), dataFrame.encodeFrame().size()),
                std::bind(&WebsocketClient::write, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
            );
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
        	std::ostringstream stream_buffer;
			std::cout << &response_buffer << std::endl;
        	stream_buffer << &response_buffer;	
			//wsFrameMessage frameData;
			//frameData.decodeFrame(std::vector<uint8_t>(stream_buffer.str().begin(), stream_buffer.str().end()));
			//std::vector<char> message(frameData.payload.begin(), frameData.payload.end());

            if (onMessageReceived)
                onMessageReceived(bytes_recvd, "none");

            asio::async_read(tcp.socket, response_buffer,
                std::bind(&WebsocketClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
            );
        }
    };
} // InternetProtocol