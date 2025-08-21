/** 
 * Copyright © 2025 Nathan Miguel
 */

#include "websocket/wsclient.h"

#include "utils/dataframe.h"
#include "utils/handshake.h"
#include "utils/net.h"

bool UWSClient::IsOpen() {
	return net.socket.is_open() && close_state.Load() == ECloseState::OPEN;
}

FTcpEndpoint UWSClient::LocalEndpoint() {
	tcp::endpoint ep = net.socket.local_endpoint();
	return FTcpEndpoint(ep);
}

FTcpEndpoint UWSClient::RemoteEndpoint() {
	tcp::endpoint ep = net.socket.remote_endpoint();
	return FTcpEndpoint(ep);
}

FErrorCode UWSClient::GetErrorCode() {
	return FErrorCode(error_code);
}

bool UWSClient::Write(const FString& Message, const FDataframe& Dataframe, const FDelegateWsClientMessageSent& Callback) {
	if (!IsOpen() || Message.IsEmpty()) return false;

	FDataframe frame = Dataframe;
	frame.Opcode = EOpcode::TEXT_FRAME;
	frame.bMask = true;
	FString payload = encode_string_payload(Message, frame);
	asio::async_write(net.socket,
					  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
					  [this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
						  Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
					  });
	return true;
}

bool UWSClient::WriteBuffer(const TArray<uint8>& Buffer, const FDataframe& Dataframe,
	const FDelegateWsClientMessageSent& Callback) {
	if (!IsOpen() || !Buffer.Num()) return false;

	FDataframe frame = Dataframe;
	frame.Opcode = EOpcode::BINARY_FRAME;
	frame.bMask = true;
	TArray<uint8> payload = encode_buffer_payload(Buffer, frame);
	asio::async_write(net.socket,
					  asio::buffer(payload.GetData(), payload.Num()),
					  [this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
						  Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
					  });
	return true;
}

bool UWSClient::Ping(const FDelegateWsClientMessageSent& Callback) {
	if (!IsOpen()) return false;

	FDataframe dataframe;
	dataframe.Opcode = EOpcode::PING;
	TArray<uint8> payload = encode_buffer_payload({}, dataframe);
	asio::async_write(net.socket,
					  asio::buffer(payload.GetData(), payload.Num()),
					  [this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
						  Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
					  });
	return true;
}

bool UWSClient::ping() {
	if (!IsOpen()) return false;

	FDataframe dataframe;
	dataframe.Opcode = EOpcode::PING;
	TArray<uint8> payload = encode_buffer_payload({}, dataframe);
	asio::async_write(net.socket,
					  asio::buffer(payload.GetData(), payload.Num()),
					  [this](const asio::error_code &ec, const size_t bytes_sent) {
					  	if (ec)
					  		AsyncTask(ENamedThreads::GameThread, [this, ec]() {
								OnError.Broadcast(FErrorCode(ec));
							});
					  });
	return true;
}

bool UWSClient::Pong(const FDelegateWsClientMessageSent& Callback) {
	if (!IsOpen()) return false;

	FDataframe dataframe;
	dataframe.Opcode = EOpcode::PONG;
	TArray<uint8> payload = encode_buffer_payload({}, dataframe);
	asio::async_write(net.socket,
					  asio::buffer(payload.GetData(), payload.Num()),
					  [this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
						  Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
					  });
	return true;
}

bool UWSClient::pong() {
	if (!IsOpen()) return false;

	FDataframe dataframe;
	dataframe.Opcode = EOpcode::PONG;
	TArray<uint8> payload = encode_buffer_payload({}, dataframe);
	asio::async_write(net.socket,
					  asio::buffer(payload.GetData(), payload.Num()),
					  [this](const asio::error_code &ec, const size_t bytes_sent) {
					  	if (ec)
					  		AsyncTask(ENamedThreads::GameThread, [this, ec]() {
								OnError.Broadcast(FErrorCode(ec));
							});
					  });
	return true;
}

bool UWSClient::Connect(const FClientBindOptions& BindOpts) {
	if (net.socket.is_open())
		return false;

	close_state.Store(ECloseState::OPEN);
	std::string addr = TCHAR_TO_UTF8(*BindOpts.Address);
	std::string port = TCHAR_TO_UTF8(*BindOpts.Port);
	net.resolver.async_resolve(BindOpts.Protocol == EProtocolType::V4 ? tcp::v4() : tcp::v6(),
								addr, port,
								[this](const asio::error_code &ec, const tcp::resolver::results_type &results) {
									resolve(ec, results);
								});

	asio::post(thread_pool(), [this]{ run_context_thread(); });
	return true;
}

void UWSClient::End(int code, const FString& reason) {
	if (close_state.Load() == ECloseState::CLOSED) return;

	if (close_state.Load() == ECloseState::OPEN) {
		// Client send close frame
		close_state.Store(ECloseState::CLOSING);
		send_close_frame(code, reason);
	} else {
		// End connection
		Close(code, reason);
	}
}

void UWSClient::Close(int code, const FString& reason) {
	if (close_state.Load() == ECloseState::CLOSED)
		return;

	close_state.Store(ECloseState::CLOSED);
	wait_close_frame_response.Store(true);

	if (net.socket.is_open()) {
		bool is_locked = mutex_error.TryLock();

		net.socket.shutdown(tcp::socket::shutdown_both, error_code);
		if (error_code) {
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
		}
			
		net.socket.close(error_code);
		if (error_code) {
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
		}
		if (is_locked)
			mutex_error.Unlock();
	}
	net.context.stop();
	net.context.restart();
	net.endpoint = tcp::endpoint();

	AsyncTask(ENamedThreads::GameThread, [this, code, reason]() {
		OnClose.Broadcast(code, reason);
	});
}

void UWSClient::start_idle_timer() {
	idle_timer->expires_after(std::chrono::seconds(5));
	idle_timer->async_wait([this](const asio::error_code &ec) {
		if (ec == asio::error::operation_aborted)
			return;

		if (close_state.Load() == ECloseState::CLOSED)
			return;

		Close(1000, "Timeout");
	});
}

void UWSClient::send_close_frame(const uint16_t code, const FString& reason, bool wait_server) {
	if (!net.socket.is_open()) {
		Close(code, reason);
		return;
	}

	FDataframe dataframe;
	dataframe.Opcode = EOpcode::CLOSE_FRAME;

	TArray<uint8> close_payload;
	close_payload.Reserve(2 + reason.Len());
	// status code
	close_payload.Add(static_cast<uint8_t>(code >> 8));
	close_payload.Add(static_cast<uint8_t>(code & 0xFF));

	// Reason
	if (!reason.IsEmpty()) {
		for (int32 i = 0; i < reason.Len(); ++i) {
			close_payload.Add(static_cast<uint8>(reason[i]));
		}
	}

	if (close_payload.GetAllocatedSize() > close_payload.Num())
		close_payload.Shrink();

	TArray<uint8> encoded_payload = encode_buffer_payload(close_payload, dataframe);
	asio::async_write(net.socket,
					  asio::buffer(encoded_payload.GetData(), encoded_payload.Num()),
					  [this, code, reason](const asio::error_code &ec, const size_t bytes_sent) {
						  close_frame_sent_cb(ec, bytes_sent, code, reason);
					  });
}

void UWSClient::close_frame_sent_cb(const asio::error_code& error, const size_t bytes_sent, const uint16_t code,
	const FString& reason) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		if (error)
			AsyncTask(ENamedThreads::GameThread, [this, error]() {
				OnError.Broadcast(FErrorCode(error));
			});
		Close(code, reason);
	}

	if (!wait_close_frame_response.Load()) {
		End(code, reason);
		return;
	}
	start_idle_timer();
	asio::async_read(net.socket,
					 recv_buffer,
					 asio::transfer_at_least(1),
					 [this](const asio::error_code &ec, const size_t bytes_recvd) {
						 read_cb(ec, bytes_recvd);
					 });
}

void UWSClient::run_context_thread() {
	FScopeLock lock(&mutex_io);
	error_code.clear();
	net.context.run();
	if (close_state.Load() == ECloseState::OPEN) {
		Close(1006, "Abnormal closure");
	}
	close_state.Store(ECloseState::CLOSED);
}

void UWSClient::resolve(const asio::error_code& error, const tcp::resolver::results_type& results) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
		return;
	}
	net.endpoint = results.begin()->endpoint();
	asio::async_connect(net.socket,
						results,
						[this](const asio::error_code &ec, const tcp::endpoint &ep) {
							conn(ec);
						});
}

void UWSClient::conn(const asio::error_code& error) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
		return;
	}

	FString request = prepare_request(Handshake, net.socket.remote_endpoint().address().to_string().c_str(),
										  net.socket.remote_endpoint().port());

	asio::async_write(net.socket, asio::buffer(TCHAR_TO_UTF8(*request), request.Len()),
					  [this](const asio::error_code &ec, const size_t bytes_sent) {
						  write_handshake_cb(ec, bytes_sent);
					  });
}

void UWSClient::write_handshake_cb(const asio::error_code& error, const size_t bytes_sent) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
		Close(1006, "Abnormal closure");
		return;
	}

	asio::async_read_until(net.socket,
						   recv_buffer, "\r\n",
						   [this](const asio::error_code &ec, const size_t bytes_received) {
							   read_handshake_cb(ec, bytes_received);
						   });
}

void UWSClient::read_handshake_cb(const asio::error_code& error, const size_t bytes_recvd) {
	if (error) {
		FScopeLock lock(&mutex_error);
		consume_recv_buffer();
		error_code = error;
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
		return;
	}

	FHttpResponse response;
	std::istream response_stream(&recv_buffer);
	std::string http_version;
	unsigned int status_code;
	std::string status_message;
	response_stream >> http_version;
	response_stream >> status_code;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
		consume_recv_buffer();
		response.Status_Code = 505;
		response.Status_Message = "HTTP Version Not Supported";
		AsyncTask(ENamedThreads::GameThread, [this, response]() {
			OnUnexpectedHandshake.Broadcast(response);
			Close(1002, "Protocol error");
		});		
		return;
	}
	response.Status_Code = status_code;
	response.Status_Message = status_message.c_str();
	if (status_code != 101 && recv_buffer.size() == 0) {
		consume_recv_buffer();
		AsyncTask(ENamedThreads::GameThread, [this, response]() {
			OnUnexpectedHandshake.Broadcast(response);
			Close(1002, "Protocol error");
		});
		return;
	}

	asio::async_read_until(net.socket,
						   recv_buffer, "\r\n\r\n",
						   std::bind(&UWSClient::read_headers, this, asio::placeholders::error, response));
}

void UWSClient::read_headers(const asio::error_code& error, FHttpResponse& response) {
	if (error) {
		FScopeLock lock(&mutex_error);
		consume_recv_buffer();
		error_code = error;
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
		return;
	}
	std::istream response_stream(&recv_buffer);
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
		res_append_header(response, header);

	consume_recv_buffer();

	if (!validate_handshake_response(Handshake, response)) {
		AsyncTask(ENamedThreads::GameThread, [this, response]() {
			OnUnexpectedHandshake.Broadcast(response);
			Close(1002, "Protocol error");
		});
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [this, response]() {
		OnConnected.Broadcast(response);
	});

	asio::async_read(net.socket,
					 recv_buffer,
					 asio::transfer_at_least(1),
					 [this](const asio::error_code &ec, const size_t bytes_recvd) {
						 read_cb(ec, bytes_recvd);
					 });
}

void UWSClient::consume_recv_buffer() {
	const size_t size = recv_buffer.size();
	if (size > 0)
		recv_buffer.consume(size);
}

void UWSClient::read_cb(const asio::error_code& error, const size_t bytes_recvd) {
	if (error) {
		FScopeLock lock(&mutex_error);
		consume_recv_buffer();
		error_code = error;
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				if (OnError.IsBound())
					OnError.Broadcast(FErrorCode(error_code));
			});
	}

    TArray<uint8> buffer;
    buffer.SetNum(bytes_recvd);
    asio::buffer_copy(asio::buffer(buffer.GetData(), bytes_recvd),
                      recv_buffer.data());

    FDataframe dataframe;
    TArray<uint8> payload{};
    if (decode_payload(buffer, payload, dataframe)) {
    	if (dataframe.bMask) {
    	    consume_recv_buffer();
    	    End(1002, "Protocol error - unexpected payload mask");
    	    return;
    	}
        switch (dataframe.Opcode) {
        case EOpcode::TEXT_FRAME:
        	AsyncTask(ENamedThreads::GameThread, [this, payload]() {
				OnMessage.Broadcast(payload, false);
			});
            break;
        case EOpcode::BINARY_FRAME:
        	AsyncTask(ENamedThreads::GameThread, [this, payload]() {
				OnMessage.Broadcast(payload, true);
			});
            break;
        case EOpcode::PING:
        	AsyncTask(ENamedThreads::GameThread, [this]() {
				OnPing.Broadcast();
			});
            pong();
            break;
        case EOpcode::PONG:
            AsyncTask(ENamedThreads::GameThread, [this, payload]() {
				OnMessage.Broadcast(payload, true);
			});
            break;
        case EOpcode::CLOSE_FRAME:
            uint16 close_code = 1000;
            FString close_reason = "Shutdown connection";
            if (payload.Num() >= 2) {
                close_code = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                if (payload.Num() > 2) {
                	close_reason = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(payload.GetData() + 2)));
                }
            }
            wait_close_frame_response.Store(close_state.Load() == ECloseState::CLOSING);
            End(close_code, close_reason);
            return;
        }
    } else {
        consume_recv_buffer();
        End(1002, "Protocol error - failed to decode payload");
        return;
    }

    consume_recv_buffer();

    if (close_state == ECloseState::OPEN) {
        asio::async_read(net.socket,
                         recv_buffer,
                         asio::transfer_at_least(1),
                         [this](const asio::error_code &ec, const size_t bytes_received) {
                             read_cb(ec, bytes_received);
                         });
    }
}





