/*
 * Copyright (c) 2023-2024 Nathan Miguel
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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpRetry, const int, attemp);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpMessageSent, const int, size);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpMessageReceived, const FUdpMessage, message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateUdpError, const int, code, const FString&, exception);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UUDPClient : public UObject
{
	GENERATED_BODY()

public:	
	virtual void BeginDestroy() override
	{
		ShouldStopContext = true;
		UDP.resolver.cancel();
		if (IsConnected())
		{
			Close();
		}
		Thread_Pool->wait();
		consume_receive_buffer();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Remote")
	void SetHost(const FString& ip, const FString& port)
	{
		Host = ip;
		Service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Local")
	FString GetLocalAdress() const
	{
		if (IsConnected())
		{
			return UDP.socket.local_endpoint().address().to_string().c_str();
		}
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Local")
	FString GetLocalPort() const
	{
		if (IsConnected())
		{
			return FString::FromInt(UDP.socket.local_endpoint().port());
		}
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Remote")
	FString GetRemoteAdress() const
	{
		if (IsConnected())
		{
			return UDP.socket.remote_endpoint().address().to_string().c_str();
		}
		return Host;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Remote")
	FString GetRemotePort() const
	{
		if (IsConnected())
		{
			return FString::FromInt(UDP.socket.remote_endpoint().port());
		}
		return Service;
	}

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void SetTimeout(uint8 value = 3) { Timeout = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	uint8 GetTimeout() const { return Timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void SetMaxAttemp(uint8 value = 3) { MaxAttemp = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	uint8 GetMaxAttemp() const { return Timeout; }

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
	bool Send(const FString& message);

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Message")
	bool SendRaw(const TArray<uint8>& buffer);

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Message")
	bool AsyncRead();

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Connection")
	bool Connect();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Connection")
	bool IsConnected() const { return UDP.socket.is_open(); }

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Connection")
	void Close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Error")
	int GetErrorCode() const { return UDP.error_code.value(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Error")
	FString GetErrorMessage() const { return UDP.error_code.message().c_str(); }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpConnection OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpRetry OnConnectionWillRetry;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpError OnError;

private:
	TUniquePtr<asio::thread_pool> Thread_Pool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	bool ShouldStopContext = false;
	FAsioUdp UDP;
	FString Host = "localhost";
	FString Service;
	uint8_t Timeout = 3;
	uint8_t MaxAttemp = 3;
	bool SplitBuffer = true;
	int MaxSendBufferSize = 1024;
	int MaxReceiveBufferSize = 1024;
	FUdpMessage RBuffer;

	void package_string(const FString& str);
	void package_buffer(const TArray<uint8>& buffer);

	void consume_receive_buffer()
	{
		if (RBuffer.RawData.Num() <= 0)
		{
			return;
		}
		RBuffer.RawData.Empty();
		RBuffer.RawData.Shrink();
		if (ShouldStopContext)
		{
			return;
		}
		RBuffer.RawData.SetNum(MaxReceiveBufferSize, true);
	}

	void run_context_thread();
	void resolve(const asio::error_code& error, const asio::ip::udp::resolver::results_type& results);
	void conn(const asio::error_code& error);
	void send_to(const asio::error_code& error, const size_t bytes_sent);
	void receive_from(const asio::error_code& error, const size_t bytes_recvd);
};
