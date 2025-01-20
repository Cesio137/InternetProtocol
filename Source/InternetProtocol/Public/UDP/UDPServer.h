// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/Commons.h"
#include "Net/Message.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "UDPServer.generated.h"
/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateUdpServerConnection);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateUdpServerTransferred, const int, BytesSent, const int, BytesRecvd);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpServerMessageSent, const FUDPEndpoint, Endpoint);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateUdpServerMessageReceived, const FUdpMessage, Message, const FUDPEndpoint, Endpoint);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpServerError, FErrorCode, ErrorCode);

UCLASS(Blueprintable, BlueprintType)
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
	UFUNCTION(BlueprintCallable, Category = "UDP||Socket")
	void SetSocket(const EProtocolType protocol = EProtocolType::V4, const int port = 3000)
	{
		ProtocolType = protocol;
		UdpPort = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Socket")
	FUDPSocket GetSocket() { return UDP.socket; }

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
	bool SendStr(const FString& message, const FUDPEndpoint& endpoint);

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Message")
	bool SendBuffer(const TArray<uint8>& buffer, const FUDPEndpoint& endpoint);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Connection")
	bool Open();

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Connection")
	void Close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Error")
	FErrorCode GetErrorCode() const
	{
		return ErrorCode;
	}

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpServerConnection OnOpen;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpServerTransferred OnBytesTransferred;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpServerMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpServerMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpServerConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpServerError OnSocketError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpServerError OnError;

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
