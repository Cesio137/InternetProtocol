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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpWillRetry, int, Attemp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpMessageSent, int, size);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateUdpMessageReceived, int, size, const FUdpMessage, message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateUdpError, int, code, const FString&, exception);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UUDPClient : public UObject
{
	GENERATED_BODY()
public:
	UUDPClient() {}

	~UUDPClient() {}
	
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
	void setTimeout(uint8 value = 4) { timeout = value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	uint8 getTimeout() const { return timeout; }
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void setMaxAttemp(uint8 value = 3) { maxAttemp = value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	uint8 getMaxAttemp() const { return timeout; }
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void setMaxBufferSize(int value = 1400) { maxBufferSize = value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	int getMaxBufferSize() const { return maxBufferSize; }
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Settings")
	void setSplitPackage(bool value = true) { splitBuffer = value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Settings")
	bool getSplitPackage() const { return splitBuffer; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Message")
	void send(const FString& message);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Connection")
	void connect();
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Connection")
	bool isConnected() const { return udp.socket.is_open(); }
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Connection")
	void close() {             
		udp.context.stop(); 
		udp.socket.close();
		OnConnectionFinished.Broadcast();
	}

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpConnection OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpWillRetry OnConnectionWillRetry;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpConnection OnConnectionFinished;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpError OnConnectionError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpError OnMessageSentError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateUdpError OnMessageReceivedError;

private:
	TUniquePtr<asio::thread_pool> pool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	std::mutex mutexIO;
	std::mutex mutexBuffer;
	FAsioUdp udp;
	FString host = "localhost";
	FString service;
	uint8_t timeout = 4;
	uint8_t maxAttemp = 3;
	bool splitBuffer = true;
	int maxBufferSize = 1400;
	FUdpMessage rbuffer;

	void runContextThread();
	void resolve(std::error_code error, asio::ip::udp::resolver::results_type results);
	void conn(std::error_code error);
	void package_buffer(const TArray<char>& buffer);
	void send_to(std::error_code error, std::size_t bytes_sent);
	void receive_from(std::error_code error, std::size_t bytes_recvd);
};
