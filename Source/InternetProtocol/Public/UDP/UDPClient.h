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
#include "UObject/NoExportTypes.h"
#include "Net/Commons.h"
#include "Net/Message.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "UDPClient.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateUdpConnection);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateUdpTransferred, const int, BytesSent, const int, BytesRecvd);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateUdpMessageSent);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpMessageReceived, const FUdpMessage, message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpError, FErrorCode, ErrorCode);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UUDPClient : public UObject
{
	GENERATED_BODY()

public:	
	virtual void BeginDestroy() override
	{
		UDP.resolver.cancel();
		if (GetSocket().Socket->is_open())
		{
			Close();
		}
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Remote")
	void SetHost(const FString& ip = "localhost", const FString& port = "3000", const EProtocolType protocol = EProtocolType::V4)
	{
		Host = ip;
		Service = port;
		ProtocolType = protocol;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Socket")
	FUDPSocket GetSocket() { return UDP.socket; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Local")
	FString GetLocalAdress() const
	{
		return UDP.socket.local_endpoint().address().to_string().c_str();
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Local")
	int GetLocalPort() const
	{
		return UDP.socket.local_endpoint().port();
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Remote")
	FString GetRemoteAdress() const
	{
		return UDP.socket.remote_endpoint().address().to_string().c_str();
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Remote")
	int GetRemotePort() const
	{
		return UDP.socket.remote_endpoint().port();
	}

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void SetMaxSendBufferSize(int value = 1024) { MaxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void SetMaxReceiveBufferSize(int value = 1024) { MaxReceiveBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	int GetMaxReceiveBufferSize() const { return MaxReceiveBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void SetSplitPackage(bool value = true) { SplitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	bool GetSplitPackage() const { return SplitBuffer; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Message")
	bool SendStr(const FString& message);

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Message")
	bool SendBuffer(const TArray<uint8>& buffer);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Connection")
	bool Connect();

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Connection")
	void Close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Error")
	FErrorCode GetErrorCode() const
	{
		FErrorCode error_code = ErrorCode;
		return error_code;
	}

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpConnection OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpTransferred OnBytesTransferred;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpError OnSocketError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FAsioUdpClient UDP;
	asio::error_code ErrorCode;
	FString Host = "localhost";
	FString Service = "3000";
	EProtocolType ProtocolType = EProtocolType::V4;
	bool SplitBuffer = true;
	int MaxSendBufferSize = 1024;
	int MaxReceiveBufferSize = 1024;
	FUdpMessage RBuffer;

	void package_string(const FString& str);
	void package_buffer(const TArray<uint8>& buffer);
	void consume_receive_buffer();
	void run_context_thread();
	void resolve(const asio::error_code& error, const asio::ip::udp::resolver::results_type& results);
	void conn(const asio::error_code& error);
	void send_to(const asio::error_code& error, const size_t bytes_sent);
	void receive_from(const asio::error_code& error, const size_t bytes_recvd);
};
