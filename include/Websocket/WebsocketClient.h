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

    	/*DATAFRAME*/
    	void setRSV1(bool value = false) { sDataFrame.rsv1 = value; }
    	bool useRSV1() const { return sDataFrame.rsv1; }
    	void setRSV2(bool value = false) { sDataFrame.rsv2 = value; }
    	bool useRSV2() const { return sDataFrame.rsv2; }
    	void setRSV3(bool value = false) { sDataFrame.rsv3 = value; }
    	bool useRSV3() const { return sDataFrame.rsv3; }
    	void setMask(bool value = true) { sDataFrame.mask = value; }
    	bool useMask() const { return sDataFrame.mask; }
    	void setOpcode(EOpcode value = EOpcode::TEXT_FRAME) { sDataFrame.opcode = value; }
    	EOpcode getOpcode() const { return sDataFrame.opcode; }

        /*MESSAGE*/
    	void send(const std::string& message)
        {
        	if (!pool || !isConnected() || message.empty())
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
            if (!pool || !isConnected() || buffer.empty())
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
    	bool splitBuffer = false;
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
		FDataFrame sDataFrame;
    	FDataFrame rDataFrame;

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

        void write_handshake(const std::error_code& error, const std::size_t bytes_sent)
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

        void read_handshake(const std::error_code& error, const std::size_t bytes_sent, std::size_t bytes_recvd)
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

			request_buffer.consume(request_buffer.size());

			// Read the remaining response headers, which are terminated by a blank line.
			asio::async_read_until(tcp.socket, response_buffer, "\r\n\r\n",
				std::bind(&WebsocketClient::read_remaining_header, this, asio::placeholders::error)
			);

            // The connection was successful
            if (onConnected)
                onConnected();
		}

		void read_remaining_header(const std::error_code& error)
		{
			response_buffer.consume(response_buffer.size());
			// Start reading remaining data until EOF.
			asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
				std::bind(&WebsocketClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
			);
		}
		
        void package_buffer(const std::vector<char>& buffer) { 
	        mutexBuffer.lock();
        	if (!splitBuffer || buffer.size() <= maxBufferSize) {
        		sDataFrame.fin = true;
        		sDataFrame.payload.assign(buffer.begin(), buffer.end());
        		encodePayload(sDataFrame.payload);
        		asio::async_write(tcp.socket, asio::buffer(sDataFrame.payload.data(), sDataFrame.payload.size()),
					std::bind(&WebsocketClient::write, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
				);
        		mutexBuffer.unlock();
        		return;
        	}

        	sDataFrame.fin = false;
        	size_t buffer_offset = 0;
        	const size_t max_size = maxBufferSize - getFrameEncodeSize(buffer) - 1;
        	while (buffer_offset < buffer.size()) {
        		sDataFrame.fin = buffer_offset < buffer.size();
        		size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
        		std::vector<uint8_t> sbuffer(buffer.begin() + buffer_offset, buffer.begin() + buffer_offset + package_size);
        		if (sbuffer.back() != '\0')
        			sbuffer.push_back('\0');
        		sDataFrame.payload = sbuffer;
        		encodePayload(sDataFrame.payload);
        		asio::async_write(tcp.socket, asio::buffer(sDataFrame.payload.data(), sDataFrame.payload.size()),
					std::bind(&WebsocketClient::write, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
				);
        		buffer_offset += package_size;
        	}
            mutexBuffer.unlock();
        }

        void write(std::error_code error, std::size_t bytes_sent) {
        	if (error) {
        		if (onError)
        			onError(error.value(), error.message());
        		sDataFrame.payload.clear();
        		return;
        	}
        	if (onMessageSent)
        		onMessageSent(bytes_sent);
        	sDataFrame.payload.clear();
        }

        void read(std::error_code error, std::size_t bytes_recvd) {
            if (error) {
                if (onError)
                    onError(error.value(), error.message());
                return;
            }
			
        	if (!decodePayload(response_buffer, rDataFrame.payload)) {
        		if (onError)
        			onError(-1, "Error: can not decoding payload!");
				response_buffer.consume(response_buffer.size());
        		asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
					std::bind(&WebsocketClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
				);
        		return;
        	}
        	std::vector<char> message(rDataFrame.payload.begin(), rDataFrame.payload.end());
            if (onMessageReceived)
                onMessageReceived(bytes_recvd, message.data());
			response_buffer.consume(response_buffer.size());
            asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
				std::bind(&WebsocketClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
			);
        }

		void encodePayload(std::vector<uint8_t>& payload) {
        	std::vector<uint8_t> buffer;
        	uint64_t payload_length = payload.size();

        	// FIN, RSV, Opcode
        	uint8_t byte1 = sDataFrame.fin ? 0x80 : 0x00;
        	byte1 |= sDataFrame.rsv1 ? (uint8_t)ERSV::RSV1 : 0x00;
        	byte1 |= sDataFrame.rsv2 ? (uint8_t)ERSV::RSV2 : 0x00;
        	byte1 |= sDataFrame.rsv3 ? (uint8_t)ERSV::RSV3 : 0x00;
        	byte1 |= ((uint8_t)sDataFrame.opcode & 0x0F);
        	buffer.push_back(byte1);

        	// Mask and payload size
        	uint8_t byte2 = sDataFrame.mask ? 0x80 : 0x00;
        	if (payload_length <= 125) {
        		byte2 |= payload_length;
        		buffer.push_back(byte2);
        	} else if (payload_length <= 65535) {
        		byte2 |= 126;
        		buffer.push_back(byte2);
        		buffer.push_back((payload_length >> 8) & 0xFF);
        		buffer.push_back(payload_length & 0xFF);
        	} else {
        		byte2 |= 127;
        		buffer.push_back(byte2);
        		for (int i = 7; i >= 0; --i) {
        			buffer.push_back((payload_length >> (8 * i)) & 0xFF);
        		}
        	}

        	std::array<uint8_t, 4> masking_key;
        	if (sDataFrame.mask) {
        		masking_key = mask_gen();
        		for (uint8_t key : masking_key) buffer.push_back(key);
        	}

        	// payload data and mask
        	for (size_t i = 0; i < payload.size(); ++i) {
        		if (sDataFrame.mask) {
        			buffer.push_back(payload[i] ^ masking_key[i % 4]);
        		} else {
        			buffer.push_back(payload[i]);
        		}
        	}

        	payload = buffer;
		}

    	std::array<uint8_t, 4> mask_gen() {
        	std::array<uint8_t, 4> maskKey;
        	std::random_device rd;
        	std::mt19937 gen(rd());
        	std::uniform_int_distribution<> dis(1, 255);

        	for (uint8_t& byte : maskKey) {
        		byte = (uint8_t)dis(gen);
        	}

        	return maskKey;
        }

    	size_t getFrameEncodeSize(const std::vector<char>& buffer) {
	        size_t size = 2;
        	if (buffer.size() <= 125) {
        		size += 0;
        	} else if (buffer.size() <= 65535) {
        		size += 2;
        	} else {
        		size += 8;
        	}

        	if (sDataFrame.mask) size += 4;

			return size;
        }

    	bool decodePayload(const asio::streambuf& encoded_payload, std::vector<uint8_t>& decoded_payload) {
        	if (encoded_payload.size() < 2) return false;
        	const char* data = asio::buffer_cast<const char*>(encoded_payload.data());
        	std::size_t size = encoded_payload.size();
			std::vector<uint8_t> encoded_buffer(data, data + size);

        	size_t pos = 0;
        	// FIN, RSV, Opcode
        	uint8_t byte1 = encoded_buffer[pos++];
        	rDataFrame.fin = byte1 & 0x80;
        	rDataFrame.rsv1 = byte1 & 0x80;
        	rDataFrame.rsv2 = byte1 & 0x40;
        	rDataFrame.rsv3 = byte1 & 0x10;
        	rDataFrame.opcode = (EOpcode)(byte1 & 0x0F);

        	// Mask and payload length
        	uint8_t byte2 = encoded_buffer[pos++];
        	rDataFrame.mask = byte2 & 0x80;
        	uint64_t payload_length = byte2 & 0x7F;
        	if (payload_length == 126) {
        		if (encoded_buffer.size() < pos + 2) return false;
        		payload_length = (encoded_buffer[pos] << 8) | encoded_buffer[pos + 1];
        		pos += 2;
        	} else if (payload_length == 127) {
        		if (encoded_buffer.size() < pos + 8) return false;
        		payload_length = 0;
        		for (int i = 0; i < 8; ++i) {
        			payload_length = (payload_length << 8) | encoded_buffer[pos + i];
        		}
        		pos += 8;
        	}

        	// Masking key
        	std::array<uint8_t, 4> masking_key;
        	if (rDataFrame.mask) {
        		if (encoded_buffer.size() < pos + 4) return false;
        		for (int i = 0; i < 4; ++i) {
        			masking_key[i] = encoded_buffer[pos++];
        		}
        	}

        	// Payload data
        	if (encoded_buffer.size() < pos + payload_length) return false;
        	decoded_payload.resize(payload_length);
        	for (size_t i = 0; i < payload_length; ++i) {
        		decoded_payload[i] = encoded_buffer[pos++];
        		if (rDataFrame.mask) {
        			decoded_payload[i] ^= masking_key[i % 4];
        		}
        	}

			return true;
        }
    };
} // InternetProtocol