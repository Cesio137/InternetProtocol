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
		void setMaxSendBufferSize(int value = 1400) { maxSendBufferSize = value; }
    	int getMaxSendBufferSize() const { return maxSendBufferSize; }
    	void setSplitPackage(bool value = true) { splitBuffer = value; }
    	bool getSplitPackage() const { return splitBuffer; }
		

        /*HANDSHAKE*/
    	void setPath(const std::string& value = "chat") { handshake.path = value; }
    	std::string getPath() const { return handshake.path; }
    	void setVersion(const std::string& value = "1.1") { handshake.version = value; }
    	std::string getVersion() const { return handshake.version; }
        void setKey(const std::string& value = "dGhlIHNhbXBsZSBub25jZQ==") { handshake.Sec_WebSocket_Key = value; }
        std::string getKey() const { return handshake.Sec_WebSocket_Key; }
        void setOrigin(const std::string& value = "client") { handshake.origin = value; }
        std::string getOrigin() const { return handshake.origin; }
        void setSecProtocol(const std::string& value = "chat, superchat") { handshake.Sec_WebSocket_Protocol = value; }
        std::string getSecProtocol() const { return handshake.Sec_WebSocket_Protocol; }
        void setSecVersion(const std::string& value = "13") { handshake.Sec_Websocket_Version = value; }
        std::string getSecVersion() const { return handshake.Sec_Websocket_Version; }

    	/*DATAFRAME*/
    	void setRSV1(bool value = false) { sDataFrame.rsv1 = value; }
    	bool useRSV1() const { return sDataFrame.rsv1; }
    	void setRSV2(bool value = false) { sDataFrame.rsv2 = value; }
    	bool useRSV2() const { return sDataFrame.rsv2; }
    	void setRSV3(bool value = false) { sDataFrame.rsv3 = value; }
    	bool useRSV3() const { return sDataFrame.rsv3; }
    	void setMask(bool value = true) { sDataFrame.mask = value; }
    	bool useMask() const { return sDataFrame.mask; }

        /*MESSAGE*/
    	void send(const std::string& message) {
	        if (!pool || !isConnected() || message.empty())
	        	return;

        	std::vector<uint8_t> text_buffer(message.begin(), message.end());
        	if (text_buffer.back() != '\0')
        		text_buffer.push_back('\0');
        	asio::post(*pool,
				std::bind(&WebsocketClient::post, this, EOpcode::TEXT_FRAME, text_buffer)
			);
        }

		void sendRaw(const std::vector<uint8_t>& buffer)
        {
            if (!pool || !isConnected() || buffer.empty())
                return;

        	asio::post(*pool,
				std::bind(&WebsocketClient::post, this, EOpcode::BINARY_FRAME, buffer)
			);
        }

    	void sendPing() {
        	if (!pool || !isConnected())
        		return;

        	std::vector<uint8_t> ping_buffer;
        	ping_buffer.resize(1);
        	if (ping_buffer.back() != '\0')
        		ping_buffer.push_back('\0');
        	asio::post(*pool,
				std::bind(&WebsocketClient::post, this, EOpcode::PING, ping_buffer)
			);
        }

    	void async_read() {
        	if (!isConnected())
        		return;

        	asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(2),
				std::bind(&WebsocketClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
			);
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
        	tcp.socket.shutdown(asio::ip::tcp::socket::shutdown_both, tcp.error_code);
        	if (tcp.error_code && onError) onError(tcp.error_code.value(), tcp.error_code.message());

        	tcp.socket.close(tcp.error_code);
        	if (tcp.error_code && onError) onError(tcp.error_code.value(), tcp.error_code.message());
        	if (onClose) onClose();
        }

        /*EVENTS*/
        std::function<void()> onConnected;
    	std::function<void(int)> onConnectionRetry;
		std::function<void()> onClose;
    	std::function<void()> onCloseNotify;
		std::function<void(std::size_t)> onMessageSent;
        std::function<void(int, const FWsMessage)> onMessageReceived;
    	std::function<void()> onPongReceived;
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
    	int maxSendBufferSize = 1400;
		FAsioTcp tcp;
		asio::streambuf request_buffer;
		asio::streambuf response_buffer;
        FHandShake handshake;
		FDataFrame sDataFrame;

        void runContextThread() {
            mutexIO.lock();
            tcp.context.restart();
            tcp.resolver.async_resolve(host, service, 
                std::bind(&WebsocketClient::resolve, this, asio::placeholders::error, asio::placeholders::endpoint)
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
							std::bind(&WebsocketClient::resolve, this, asio::placeholders::error, asio::placeholders::endpoint)
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

    	void post(EOpcode opcode, const std::vector<uint8_t>& buffer) {
        	mutexBuffer.lock();
	        sDataFrame.opcode = opcode;
        	if (opcode == EOpcode::TEXT_FRAME || opcode == EOpcode::BINARY_FRAME) {
        		package_buffer(buffer);
        	} else if (opcode == EOpcode::PING) {
        		std::vector<uint8_t> pong_buffer = buffer;
        		encodePayload(pong_buffer);
        		asio::async_write(tcp.socket, asio::buffer(pong_buffer.data(), pong_buffer.size()),
					std::bind(&WebsocketClient::write, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
				);
        		mutexBuffer.unlock();
        		return;
        	}
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
            request << "GET /" + handshake.path + " HTTP/" + handshake.version + "\r\n";
            request << "Host: " + host + "\r\n";
            request << "Upgrade: websocket\r\n";
            request << "Connection: Upgrade\r\n";
            request << "Sec-WebSocket-Key: " + handshake.Sec_WebSocket_Key +"\r\n";
            request << "Origin: " + handshake.origin +"\r\n";
            request << "Sec-WebSocket-Protocol: " + handshake.Sec_WebSocket_Protocol +"\r\n";
            request << "Sec-WebSocket-Version: " + handshake.Sec_Websocket_Version +"\r\n";
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
				close();
				return;
			}
			if (status_code != 101)
			{
				if (onError)
					onError(status_code, "Invalid status code.");
				close();
				return;
			}

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
			request_buffer.consume(request_buffer.size());
			response_buffer.consume(response_buffer.size());
			// Start reading data.

			asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(2),
				std::bind(&WebsocketClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
			);
		}
		
        void package_buffer(const std::vector<uint8_t>& buffer) {
        	FWsMessage sBuffer;
        	if (!splitBuffer || buffer.size() + getFrameEncodeSize(buffer) <= maxSendBufferSize) {
        		sDataFrame.fin = true;
                sBuffer.payload.resize(buffer.size());
        		sBuffer.payload.assign(buffer.begin(), buffer.end());
        		encodePayload(sBuffer.payload);
        		asio::async_write(tcp.socket, asio::buffer(sBuffer.payload.data(), sBuffer.payload.size()),
					std::bind(&WebsocketClient::write, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
				);
        		mutexBuffer.unlock();
        		return;
        	}

        	sBuffer.dataFrame.fin = false;
        	size_t buffer_offset = 0;
        	const size_t max_size = maxSendBufferSize - getFrameEncodeSize(buffer) - 1;
        	while (buffer_offset < buffer.size()) {
        		sBuffer.dataFrame.fin = buffer_offset < buffer.size();
        		size_t package_size = std::min(max_size, buffer.size() - buffer_offset);
        		std::vector<uint8_t> sbuffer(buffer.begin() + buffer_offset, buffer.begin() + buffer_offset + package_size);
        		if (sbuffer.back() != '\0')
        			sbuffer.push_back('\0');
        		sBuffer.payload = sbuffer;
        		encodePayload(sBuffer.payload, sBuffer.dataFrame.fin);
        		asio::async_write(tcp.socket, asio::buffer(sBuffer.payload, sBuffer.payload.size()),
					std::bind(&WebsocketClient::write, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
				);
        		buffer_offset += package_size;
        		if (sDataFrame.opcode != EOpcode::FRAME_CON) sDataFrame.opcode = EOpcode::FRAME_CON;
        	}
            mutexBuffer.unlock();
        }

        void write(const std::error_code& error, const std::size_t bytes_sent) {
        	if (error) {
        		if (onError)
        			onError(error.value(), error.message());
				
        		return;
        	}

        	if (onMessageSent)
        		onMessageSent(bytes_sent);
        }

        void read(const std::error_code& error, const size_t bytes_recvd) {
            if (error) {
                if (onError)
                    onError(error.value(), error.message());
                return;
            }

			FWsMessage rDataFrame;
        	if (!decodePayload(response_buffer, rDataFrame)) {
        		if (onError)
        			onError(-1, "Error: can not decoding payload!");

        		asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(2),
					std::bind(&WebsocketClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
				);
        		return;
        	}

        	if (rDataFrame.dataFrame.opcode == EOpcode::PONG) {
				if (onPongReceived)
					onPongReceived();
        	} else if (rDataFrame.dataFrame.opcode == EOpcode::CONNECTION_CLOSE) {
				if (onCloseNotify)
					onCloseNotify();
			} else {
        		if (onMessageReceived)
					onMessageReceived(bytes_recvd, rDataFrame);
        	}

            asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(2),
				std::bind(&WebsocketClient::read, this, asio::placeholders::error, asio::placeholders::bytes_transferred)
			);
        }

		void encodePayload(std::vector<uint8_t>& payload, bool fin = true) {
        	std::vector<uint8_t> buffer;
        	uint64_t payload_length = payload.size();

        	// FIN, RSV, Opcode
        	uint8_t byte1 = fin ? 0x80 : 0x00;
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
        	std::uniform_int_distribution<> dis(0, 255);

        	for (uint8_t& byte : maskKey) {
        		byte = (uint8_t)dis(gen);
        	}

        	return maskKey;
        }

    	size_t getFrameEncodeSize(const std::vector<uint8_t>& buffer) {
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

    	bool decodePayload(asio::streambuf& stream_buffer, FWsMessage& data_frame) {
        	if (stream_buffer.size() < 2) return false;
        	std::size_t size = stream_buffer.size();
			std::istream istream(&stream_buffer);
			std::vector<uint8_t> encoded_buffer;
			encoded_buffer.reserve(size);
			uint8_t stream_byte;
			while (istream >> stream_byte) {
				encoded_buffer.push_back(stream_byte);
			}

        	size_t pos = 0;
        	// FIN, RSV, Opcode
        	uint8_t byte1 = encoded_buffer[pos++];
        	data_frame.dataFrame.fin = byte1 & 0x80;
        	data_frame.dataFrame.rsv1 = byte1 & 0x80;
        	data_frame.dataFrame.rsv2 = byte1 & 0x40;
        	data_frame.dataFrame.rsv3 = byte1 & 0x10;
        	data_frame.dataFrame.opcode = (EOpcode)(byte1 & 0x0F);

        	// Mask and payload length
        	uint8_t byte2 = encoded_buffer[pos++];
        	data_frame.dataFrame.mask = byte2 & 0x80;
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
			data_frame.dataFrame.length = payload_length;

        	// Masking key
        	if (data_frame.dataFrame.mask) {
        		if (encoded_buffer.size() < pos + 4) return false;
        		for (int i = 0; i < 4; ++i) {
        			data_frame.dataFrame.masking_key[i] = encoded_buffer[pos++];
        		}
        	}

        	// Payload data
        	if (encoded_buffer.size() < pos + payload_length) return false;
        	data_frame.payload.reserve(payload_length);
        	for (size_t i = 0; i < payload_length; ++i) {
        		data_frame.payload.push_back(encoded_buffer[pos++]);
        		if (data_frame.dataFrame.mask) {
        			data_frame.payload[i] ^= data_frame.dataFrame.masking_key[i % 4];
        		}
        	}

			return true;
        }
    };
} // InternetProtocol