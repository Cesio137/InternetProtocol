// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/Net/Message.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "UDPClient.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateUdpConnection);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpRetry, const int, attemp);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpMessageSent, const int, size);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateUdpMessageReceived, int, size, const FUdpMessage, message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateUdpError, const int, code, const FString&, exception);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UUDPClient : public UObject
{
	GENERATED_BODY()

public:
	UUDPClient()
	{
	}

	virtual ~UUDPClient() override
	{
		udp.context.stop();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Host")
	void setHost(const FString& ip, const FString& port)
	{
		host = ip;
		service = port;
	}

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Host")
	FString getHost() const { return host; }

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Host")
	FString getPort() const { return service; }

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void setTimeout(uint8 value = 3) { timeout = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	uint8 getTimeout() const { return timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void setMaxAttemp(uint8 value = 3) { maxAttemp = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	uint8 getMaxAttemp() const { return timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void setMaxSendBufferSize(int value = 1024) { maxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	int getMaxSendBufferSize() const { return maxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void setMaxReceiveBufferSize(int value = 1024) { maxReceiveBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	int getMaxReceiveBufferSize() const { return maxReceiveBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void setSplitPackage(bool value = true) { splitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	bool getSplitPackage() const { return splitBuffer; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Message")
	void send(const FString& message);

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Message")
	void sendRaw(const TArray<uint8>& buffer);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Connection")
	void connect();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Connection")
	bool isConnected() const { return udp.socket.is_open(); }

	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Connection")
	void close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Error")
	int getErrorCode() const { return udp.error_code.value(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Error")
	FString getErrorMessage() const { return udp.error_code.message().data(); }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpConnection OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpConnection OnConnectionClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpRetry OnConnectionWillRetry;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpError OnError;

private:
	TUniquePtr<asio::thread_pool> pool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	std::mutex mutexIO;
	std::mutex mutexBuffer;
	FAsioUdp udp;
	FString host = "localhost";
	FString service;
	uint8_t timeout = 3;
	uint8_t maxAttemp = 3;
	bool splitBuffer = true;
	int maxSendBufferSize = 1024;
	int maxReceiveBufferSize = 1024;
	FUdpMessage rbuffer;

	void package_string(const FString& str);
	void package_buffer(const TArray<uint8>& buffer);

	void consume_receive_buffer()
	{
		rbuffer.RawData.Empty();
		rbuffer.RawData.SetNum(maxReceiveBufferSize);
	}

	void run_context_thread();
	void resolve(const std::error_code& error, asio::ip::udp::resolver::results_type results);
	void conn(const std::error_code& error);
	void send_to(const std::error_code& error, const size_t bytes_sent);
	void receive_from(const std::error_code& error, const size_t bytes_recvd);
};
