/*
 * Copyright (c) 2023-2025 Nathan Miguel
 *
 * InternetProtocol is free library: you can redistribute it and/or modify it under the terms
 * of the GNU Affero General Public License as published by the Free Software Foundation,
 * version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
*/

#pragma once

#include "CoreMinimal.h"
#include "Net/Commons.h"
#include "Library/InternetProtocolFunctionLibrary.h"
#include "WSServer.generated.h"

/**
 * 
 */

UCLASS(Blueprintable, BlueprintType, Category = "IP|Websocket")
class INTERNETPROTOCOL_API UWSServer : public UObject
{
	GENERATED_BODY()

public:
	UWSServer()
	{
		ResponseHandshake.Headers.Add("Connection", "Upgrade");
		ResponseHandshake.Headers.Add("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
		ResponseHandshake.Headers.Add("Upgrade", "websocket");
		SDataFrame.mask = false;
	}

	virtual void BeginDestroy() override
	{
		if (TCP.acceptor.is_open())
		{
			Close();
		}
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Remote")
	void SetSocket(const EProtocolType Protocol = EProtocolType::V4, const int Port = 3000,
	               const int MaxListenningConnection = 2147483647)
	{
		TcpProtocol = Protocol;
		TcpPort = Port;
		Backlog = MaxListenningConnection;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Acceptor")
	FTCPAcceptor GetAcceptor()
	{
		return TCP.acceptor;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Socket")
	const TArray<FTCPSocket> GetSockets()
	{
		TArray<FTCPSocket> sockets;
		for (const socket_ptr& socket_ref : TCP.sockets)
		{
			sockets.Add(socket_ref);
		}
		return sockets;
	}

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Settings")
	void SetMaxSendBufferSize(int Value = 1400) { MaxSendBufferSize = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Settings")
	void SetSplitPackage(bool Value = true) { SplitBuffer = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Settings")
	bool GetSplitPackage() const { return SplitBuffer; }


	/*HANDSHAKE*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Handshake")
	void AppendHeader(const FString& Key, const FString& Value) { ResponseHandshake.Headers[Key] = Value; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Handshake")
	void ClearHeaders() { ResponseHandshake.Headers.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Handshake")
	void RemoveHeader(const FString& Key)
	{
		if (!ResponseHandshake.Headers.Contains(Key))
		{
			return;
		}
		ResponseHandshake.Headers.Remove(Key);
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Handshake")
	bool HasHeader(const FString& Key) { return ResponseHandshake.Headers.Contains(Key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Handshake")
	FString GetHeader(const FString& Key) { return ResponseHandshake.Headers[Key]; }

	/*DATAFRAME*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Dataframe")
	void SetRSV1(bool Value = false) { SDataFrame.rsv1 = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Dataframe")
	bool UseRSV1() const { return SDataFrame.rsv1; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Dataframe")
	void SetRSV2(bool Value = false) { SDataFrame.rsv2 = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Dataframe")
	bool UseRSV2() const { return SDataFrame.rsv2; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Dataframe")
	void SetRSV3(bool Value = false) { SDataFrame.rsv3 = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Dataframe")
	bool UseRSV3() const { return SDataFrame.rsv3; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Dataframe")
	void SetMask(bool Value = true) { SDataFrame.mask = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Dataframe")
	bool UseMask() const { return SDataFrame.mask; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Message")
	bool SendHandshakeTo(const FServerRequest &Request, FServerResponse Response, const FTCPSocket& Socket);
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Message")
	bool SendHandshakeErrorTo(const int StatusCode, const FTCPSocket &Socket);
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Message")
	bool SendStrTo(const FString& Message, const FTCPSocket& Socket);
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Message")
	bool SendBufferTo(const TArray<uint8> Buffer, const FTCPSocket& Socket);
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Message")
	bool SendPingTo(const FTCPSocket& Socket);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Connection")
	bool Open();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Connection")
	void Close();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Connection")
	void DisconnectSocket(const FTCPSocket& Socket);

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Error")
	FErrorCode GetErrorCode() const { return ErrorCode; }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsSocketHandshake OnSocketAccepted;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateBytesTransferred OnBytesTransferred;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsSocketMessage OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateConnection OnPongReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateConnection OnCloseNotify;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateHttpDataError OnHandshakeFail;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateTcpSocketError OnSocketDisconnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateTcpSocketError OnSocketError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FAsioTcpServer TCP;
	asio::error_code ErrorCode;
	EProtocolType TcpProtocol = EProtocolType::V4;
	uint16_t TcpPort = 3000;
	int Backlog = 2147483647;
	bool SplitBuffer = false;
	int MaxSendBufferSize = 1400;
	FServerRequest RequestHandshake;
	FServerResponse ResponseHandshake;
	TMap<socket_ptr, TSharedPtr<asio::streambuf>> ListenerBuffers;
	FDataFrame SDataFrame;

	void post_string(const FString& str, const socket_ptr& socket);
	void post_buffer(EOpcode opcode, const TArray<uint8>& buffer, const socket_ptr& socket);
	void package_string(const FString& str, const socket_ptr& socket);
	std::string encode_string_payload(const std::string& payload);
	void package_buffer(const TArray<uint8>& buffer, const socket_ptr& socket);
	std::vector<uint8> encode_buffer_payload(const std::vector<uint8>& payload);
	size_t get_frame_encode_size(const size_t buffer_size);
	std::array<uint8, 4> mask_gen();
	bool decode_payload(FWsMessage& data_frame, const socket_ptr& socket);
	TArray<uint8> sha1(const FString& input);
	FString base64_encode(const uint8_t* input, size_t length);
	FString generate_accept_key(const FString& sec_websocket_key);
	void package_handshake(const FServerRequest& req, FServerResponse& res, const socket_ptr& socket,
	                       const uint32_t status_code = 101);
	void package_handshake_error(const uint32_t status_code, const socket_ptr& socket);
	void consume_listening_buffer(const socket_ptr& socket);
	void run_context_thread();
	void accept(const asio::error_code& error, socket_ptr& socket);
	void read_handshake(const std::error_code& error, const size_t bytes_recvd, const socket_ptr& socket);
	void read_headers(const std::error_code& error, FServerRequest request, const socket_ptr& socket);
	void write_handshake(const std::error_code& error, const size_t bytes_sent, const socket_ptr& socket,
	                     const uint32_t status_code);
	void write(const std::error_code& error, const size_t bytes_sent, const socket_ptr& socket);
	void read(const std::error_code& error, const size_t bytes_recvd, const socket_ptr& socket);
};
