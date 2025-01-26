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
#include "UDPServer.generated.h"

/**
 * 
 */

UCLASS(Blueprintable, BlueprintType, Category = "IP|UDP")
class INTERNETPROTOCOL_API UUDPServer : public UObject
{
	GENERATED_BODY()
public:
	virtual void BeginDestroy() override
	{
		if (UDP.socket.is_open()) Close();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP|UDP|Socket")
	void SetSocket(const EProtocolType Protocol = EProtocolType::V4, const int Port = 3000)
	{
		ProtocolType = Protocol;
		UdpPort = Port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|UDP|Socket")
	FUDPSocket GetSocket() { return UDP.socket; }

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP|UDP|Settings")
	void SetMaxSendBufferSize(int Value = 1024) { MaxSendBufferSize = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|UDP|Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP|UDP|Settings")
	void SetMaxReceiveBufferSize(int Value = 1024) { MaxReceiveBufferSize = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|UDP|Settings")
	int GetMaxReceiveBufferSize() const { return MaxReceiveBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP|UDP|Settings")
	void SetSplitPackage(bool Value = true) { SplitBuffer = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|UDP|Settings")
	bool GetSplitPackage() const { return SplitBuffer; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP|UDP|Message")
	bool SendStrTo(const FString& Message, const FUDPEndpoint& Endpoint);

	UFUNCTION(BlueprintCallable, Category = "IP|UDP|Message")
	bool SendBufferTo(const TArray<uint8>& Buffer, const FUDPEndpoint& Endpoint);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP|UDP|Connection")
	bool Open();

	UFUNCTION(BlueprintCallable, Category = "IP|UDP|Connection")
	void Close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|UDP|Error")
	FErrorCode GetErrorCode() const
	{
		return ErrorCode;
	}

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateBytesTransferred OnBytesTransferred;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateUdpMessage OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FAsioUdpServer UDP;
	asio::error_code ErrorCode;
	EProtocolType ProtocolType = EProtocolType::V4;
	int UdpPort = 3000;
	bool SplitBuffer = true;
	int MaxSendBufferSize = 1024;
	int MaxReceiveBufferSize = 1024;
	FUdpMessage RBuffer;

	void package_string(const FString& str, const FUDPEndpoint& endpoint);
	void package_buffer(const TArray<uint8>& buffer, const FUDPEndpoint& endpoint);
	void consume_receive_buffer();
	void run_context_thread();
	void send_to(const asio::error_code& error, const size_t bytes_sent, const FUDPEndpoint& endpoint);
	void receive_from(const asio::error_code& error, const size_t bytes_recvd);
};
