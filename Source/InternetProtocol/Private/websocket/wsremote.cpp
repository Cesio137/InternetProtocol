/** 
 * Copyright © 2025 Nathan Miguel
 */

#include "websocket/wsremote.h"

#include "utils/dataframe.h"
#include "utils/handshake.h"
#include "utils/net.h"
#include "utils/utils.h"

void UWSRemote::BeginDestroy() {
	is_being_destroyed = true;
	if (socket.IsValid()) {
		if (socket->is_open())
			Close(1000, "Shutdown");
	}
		
	UObject::BeginDestroy();
}

void UWSRemote::Construct(asio::io_context& io_context) {
	if (!IsRooted()) AddToRoot();
	socket = MakeUnique<tcp::socket>(io_context);
}

void UWSRemote::Destroy() {
	if (IsRooted()) {
		RemoveFromRoot();
	}
	socket.Reset();
}

bool UWSRemote::IsOpen() {
	if (!socket.IsValid()) return false;
	return socket->is_open() && close_state.Load() == ECloseState::OPEN;
}

FTcpEndpoint UWSRemote::LocalEndpoint() {
	if (!socket.IsValid()) return FTcpEndpoint();
	tcp::endpoint ep = socket->local_endpoint();
	return FTcpEndpoint(ep);
}

FTcpEndpoint UWSRemote::RemoteEndpoint() {
	if (!socket.IsValid()) return FTcpEndpoint();
	tcp::endpoint ep = socket->remote_endpoint();
	return FTcpEndpoint(ep);
}

tcp::socket& UWSRemote::get_socket() {
	return *socket;
}

FErrorCode UWSRemote::GetErrorCode() {
	return FErrorCode(error_code);
}

bool UWSRemote::Write(const FString& Message, const FDataframe& Dataframe, const FDelegateWsRemoteMessageSent& Callback) {
	if (!IsOpen() || Message.IsEmpty()) return false;

	FDataframe frame = Dataframe;
	frame.Opcode = EOpcode::TEXT_FRAME;
	frame.bMask = false;
	FString payload = encode_string_payload(Message, frame);
	TArray<uint8> bytes = UUtilsFunctionLibrary::StringToByteArray(payload);
	asio::async_write(*socket,
					  asio::buffer(bytes.GetData(), bytes.Num()),
					  [this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
					  		AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
							  if (!is_being_destroyed)
							  	Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
					  		});	
					  });
	return true;
}

bool UWSRemote::WriteBuffer(const TArray<uint8>& Buffer, const FDataframe& Dataframe,
	const FDelegateWsRemoteMessageSent& Callback) {
	if (!IsOpen() || !Buffer.Num()) return false;

	FDataframe frame = Dataframe;
	frame.Opcode = EOpcode::BINARY_FRAME;
	frame.bMask = false;
	TArray<uint8> payload = encode_buffer_payload(Buffer, frame);
	asio::async_write(*socket,
					  asio::buffer(payload.GetData(), payload.Num()),
					  [this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
					  		AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
								if (!is_being_destroyed)
						  			Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
							});
					  });
	return true;
}

bool UWSRemote::Ping(const FDelegateWsRemoteMessageSent& Callback) {
	if (!IsOpen()) return false;

	FDataframe dataframe;
	dataframe.Opcode = EOpcode::PING;
	dataframe.bMask = false;
	TArray<uint8> payload = encode_buffer_payload({}, dataframe);
	asio::async_write(*socket,
					  asio::buffer(payload.GetData(), payload.Num()),
					  [this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
					  		AsyncTask(ENamedThreads::GameThread, [this, Callback, ec, bytes_sent]() {
					  			if (!is_being_destroyed)
						  			Callback.ExecuteIfBound(FErrorCode(ec), bytes_sent);
					  		});
					  });
	return true;
}

bool UWSRemote::ping() {
	if (!IsOpen()) return false;

	FDataframe dataframe;
	dataframe.Opcode = EOpcode::PING;
	dataframe.bMask = false;
	TArray<uint8> payload = encode_buffer_payload({}, dataframe);
	asio::async_write(*socket,
					  asio::buffer(payload.GetData(), payload.Num()),
					  [this](const asio::error_code &ec, const size_t bytes_sent) {
					  	if (ec)
					  		AsyncTask(ENamedThreads::GameThread, [this, ec]() {
					  			if (!is_being_destroyed)
									OnError.Broadcast(FErrorCode(ec));
							});
					  });
	return true;
}

bool UWSRemote::Pong(const FDelegateWsRemoteMessageSent& Callback) {
	if (!IsOpen()) return false;

	FDataframe dataframe;
	dataframe.Opcode = EOpcode::PONG;
	dataframe.bMask = false;
	TArray<uint8> payload = encode_buffer_payload({}, dataframe);
	asio::async_write(*socket,
					  asio::buffer(payload.GetData(), payload.Num()),
					  [this, Callback](const asio::error_code &ec, const size_t bytes_sent) {
					  		if (ec)
					  			AsyncTask(ENamedThreads::GameThread, [this, ec]() {
									  if (!is_being_destroyed)
								  		OnError.Broadcast(FErrorCode(ec));
					  			});
					  });
	return true;
}

bool UWSRemote::pong() {
	if (!IsOpen()) return false;

	FDataframe dataframe;
	dataframe.Opcode = EOpcode::PONG;
	dataframe.bMask = false;
	TArray<uint8> payload = encode_buffer_payload({}, dataframe);
	asio::async_write(*socket,
					  asio::buffer(payload.GetData(), payload.Num()),
					  [this](const asio::error_code &ec, const size_t bytes_sent) {
					  	if (ec)
					  		AsyncTask(ENamedThreads::GameThread, [this, ec]() {
					  			if (!is_being_destroyed)
									OnError.Broadcast(FErrorCode(ec));
							});
					  });
	return true;
}

void UWSRemote::connect() {
	close_state.Store(ECloseState::OPEN);
	
	asio::async_read_until(*socket,
							   recv_buffer, "\r\n",
							   [this](const asio::error_code &ec, const size_t bytes_received) {
								   read_handshake_cb(ec, bytes_received);
							   });
}

void UWSRemote::End(int code, const FString& reason) {
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

void UWSRemote::Close(int code, const FString& reason) {
	if (!socket.IsValid()) return;
	if (close_state.Load() == ECloseState::CLOSED)
		return;

	close_state.Store(ECloseState::CLOSED);
	wait_close_frame_response.Store(true);

	if (socket->is_open()) {
		bool is_locked = mutex_error.TryLock();

		socket->shutdown(tcp::socket::shutdown_both, error_code);
		if (error_code) {
			AsyncTask(ENamedThreads::GameThread, [this]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error_code));
			});
		}
			
		socket->close(error_code);
		if (error_code) {
			AsyncTask(ENamedThreads::GameThread, [this]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error_code));
			});
		}

		AsyncTask(ENamedThreads::GameThread, [this, code, reason]() {
			if (!is_being_destroyed)
				OnClose.Broadcast(code, reason);
			if (on_close) on_close();
		});
		
		if (is_locked)
			mutex_error.Unlock();
	}
}

void UWSRemote::start_idle_timer() {
	idle_timer->expires_after(std::chrono::seconds(5));
	idle_timer->async_wait([this](const asio::error_code &ec) {
		if (ec == asio::error::operation_aborted)
			return;

		if (close_state.Load() == ECloseState::CLOSED)
			return;

		Close(1000, "Timeout");
	});
}

void UWSRemote::send_close_frame(const uint16_t code, const FString& reason, bool wait_server) {
	if (!socket->is_open()) {
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
	asio::async_write(*socket,
					  asio::buffer(encoded_payload.GetData(), encoded_payload.Num()),
					  [this, code, reason](const asio::error_code &ec, const size_t bytes_sent) {
						  close_frame_sent_cb(ec, bytes_sent, code, reason);
					  });
}

void UWSRemote::close_frame_sent_cb(const asio::error_code& error, const size_t bytes_sent, const uint16_t code,
	const FString& reason) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		if (error)
			AsyncTask(ENamedThreads::GameThread, [this, error]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error));
			});
		Close(code, reason);
	}

	if (!wait_close_frame_response.Load()) {
		End(code, reason);
		return;
	}
	start_idle_timer();
	asio::async_read(*socket,
					 recv_buffer,
					 asio::transfer_at_least(1),
					 [this](const asio::error_code &ec, const size_t bytes_recvd) {
						 read_cb(ec, bytes_recvd);
					 });
}

void UWSRemote::read_handshake_cb(const asio::error_code& error, const size_t bytes_recvd) {
	if (error) {
		FScopeLock lock(&mutex_error);
		consume_recv_buffer();
		error_code = error;
		AsyncTask(ENamedThreads::GameThread, [this, error]() {
			if (!is_being_destroyed) {
				OnError.Broadcast(FErrorCode(error));
				OnClose.Broadcast(1002, "Protocol error");
			}
			if (on_close) on_close();
		});
		return;
	}

	FHttpRequest request;
	FHttpResponse response;
	std::istream response_stream(&recv_buffer);
	std::string method;
	std::string path;
	std::string version;
	response_stream >> method;
	response_stream >> path;
	response_stream >> version;
	version.erase(0, 5);

	request.Method = string_to_request_method(method.c_str());
	request.Path = path.c_str();

	if (method != "GET") {
		consume_recv_buffer();
		response.Status_Code = 405;
		response.Status_Message = "Method Not Allowed";
		FString payload = prepare_response(response);
		asio::async_write(*socket,
						  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
						  [&](const asio::error_code &ec, const size_t bytes_sent) {
							  Close(1002, "Protocol error");
						  });
		AsyncTask(ENamedThreads::GameThread, [this, request]() {
			OnUnexpectedHandshake.Broadcast(request);
		});
		return;
	}

	if (version != "1.1") {
		consume_recv_buffer();
		response.Status_Code = 505;
		response.Status_Message = "HTTP Version Not Supported";
		FString payload = prepare_response(response);
		asio::async_write(*socket,
						  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
						  [&](const asio::error_code &ec, const size_t bytes_sent) {
							  Close(1002, "Protocol error");
						  });
		AsyncTask(ENamedThreads::GameThread, [this, request]() {
			OnUnexpectedHandshake.Broadcast(request);
		});
		return;
	}

	recv_buffer.consume(2);
	asio::async_read_until(*socket,
						   recv_buffer, "\r\n\r\n",
						   std::bind(&UWSRemote::read_headers, this, asio::placeholders::error, request));
}

void UWSRemote::read_headers(const asio::error_code& error, FHttpRequest& request) {
	if (error) {
		FScopeLock lock(&mutex_error);
		consume_recv_buffer();
		error_code = error;
		AsyncTask(ENamedThreads::GameThread, [this, error]() {
			if (!is_being_destroyed) {
				OnError.Broadcast(FErrorCode(error));
				OnClose.Broadcast(1002, "Protocol error");
			}
			if (on_close) on_close();
		});
		return;
	}
	std::istream response_stream(&recv_buffer);
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
		req_append_header(request, header);

	consume_recv_buffer();

	if (!validate_handshake_request(request, handshake)) {
		handshake.Headers.Remove("Upgrade");
		handshake.Headers.Remove("Connection");
		handshake.Headers.Remove("Sec-WebSocket-Accept");
		FString payload = prepare_response(handshake);
		asio::async_write(*socket,
						  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
						  [&, request](const asio::error_code &ec, const size_t bytes_sent) {
						  	AsyncTask(ENamedThreads::GameThread, [this, request]() {
								OnUnexpectedHandshake.Broadcast(request);
							});
						  	Close(1002, "Protocol error");
						  });
		
		return;
	}

	FString key = request.Headers["sec-websocket-key"];
	FString accept = generate_accept_key(key);
	handshake.Headers.Add("Sec-WebSocket-Accept", accept);
	FString payload = prepare_response(handshake);
	asio::async_write(*socket,
					  asio::buffer(TCHAR_TO_UTF8(*payload), payload.Len()),
					  [this, request](const asio::error_code &ec, const size_t bytes_sent) {
						  write_handshake_cb(ec, bytes_sent, request);
					  });
}

void UWSRemote::write_handshake_cb(const asio::error_code& error, const size_t bytes_sent, const FHttpRequest& request) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		AsyncTask(ENamedThreads::GameThread, [this, error]() {
			if (!is_being_destroyed) {
				OnError.Broadcast(FErrorCode(error));
				OnClose.Broadcast(1002, "Protocol error");
			}
			if (on_close) on_close();
		});
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [this, request]() {
		OnConnected.Broadcast(request);
	});

	asio::async_read(*socket,
					 recv_buffer,
					 asio::transfer_at_least(1),
					 [&](const asio::error_code &ec, const size_t bytes_recvd) {
						 read_cb(ec, bytes_recvd);
					 });
}

void UWSRemote::consume_recv_buffer() {
	const size_t size = recv_buffer.size();
	if (size > 0)
		recv_buffer.consume(size);
}

void UWSRemote::read_cb(const asio::error_code& error, const size_t bytes_recvd) {
	if (error) {
		FScopeLock lock(&mutex_error);
		consume_recv_buffer();
		error_code = error;
		AsyncTask(ENamedThreads::GameThread, [this, error]() {
			if (!is_being_destroyed) {
				OnError.Broadcast(FErrorCode(error));
				OnClose.Broadcast(1002, "Protocol error");
			}
			if (on_close) on_close();
		});
		return;
	}

    TArray<uint8> buffer;
    buffer.SetNum(bytes_recvd);
    asio::buffer_copy(asio::buffer(buffer.GetData(), bytes_recvd),
                      recv_buffer.data());

    FDataframe dataframe;
    TArray<uint8> payload{};
    if (decode_payload(buffer, payload, dataframe)) {
    	if (!dataframe.bMask) {
    	    consume_recv_buffer();
    	    End(1002, "Protocol error - unexpected payload mask");
    	    return;
    	}
        switch (dataframe.Opcode) {
        case EOpcode::TEXT_FRAME:
        	AsyncTask(ENamedThreads::GameThread, [this, payload]() {
        		if (!is_being_destroyed)
					OnMessage.Broadcast(payload, false);
			});
            break;
        case EOpcode::BINARY_FRAME:
        	AsyncTask(ENamedThreads::GameThread, [this, payload]() {
        		if (!is_being_destroyed)
					OnMessage.Broadcast(payload, true);
			});
            break;
        case EOpcode::PING:
        	AsyncTask(ENamedThreads::GameThread, [this]() {
        		if (!is_being_destroyed)
					OnPing.Broadcast();
			});
            pong();
            break;
        case EOpcode::PONG:
            AsyncTask(ENamedThreads::GameThread, [this, payload]() {
            	if (!is_being_destroyed)
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
        asio::async_read(*socket,
                         recv_buffer,
                         asio::transfer_at_least(1),
                         [this](const asio::error_code &ec, const size_t bytes_received) {
                             read_cb(ec, bytes_received);
                         });
    }
}