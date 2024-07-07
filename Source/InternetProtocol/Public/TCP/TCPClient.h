// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/Net/Message.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "TCPClient.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateTcpConnection);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpWillRetry, int, Attemp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpMessageSent, int, size);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpMessageReceived, int, size, const FTcpMessage, message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpError, int, code, const FString&, exception);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UTCPClient : public UObject
{
	GENERATED_BODY()
public:
	UTCPClient() {}
	~UTCPClient() {}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Host")
	void setHost(const FString& ip, const FString& port)
	{
		host = ip;
		service = port;
	}
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Host")
	FString getHost() const { return host; }
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Host")
	FString getPort() const { return service; }

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setTimeout(uint8 value = 4) { timeout = value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	uint8 getTimeout() const { return timeout; }
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setMaxAttemp(uint8 value = 3) { maxAttemp = value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	uint8 getMaxAttemp() const { return timeout; }
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setMaxBufferSize(int value = 1400) { maxBufferSize = value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	int getMaxBufferSize() const { return maxBufferSize; }
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setSplitPackage(bool value = true) { splitBuffer = value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	bool getSplitPackage() const { return splitBuffer; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	void send(const FString& message);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	void connect();
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	bool isConnected() const { return tcp.socket.is_open(); }
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	void close() {             
		tcp.context.stop(); 
		tcp.socket.close();
		OnConnectionFinished.Broadcast();
	}

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpConnection OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpWillRetry OnConnectionWillRetry;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpConnection OnConnectionFinished;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpError OnConnectionError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpError OnMessageSentError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpError OnMessageReceivedError;

private:
	TUniquePtr<asio::thread_pool> pool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	std::mutex mutexIO;
	std::mutex mutexBuffer;
	FAsioTcp tcp;
	FString host = "localhost";
	FString service;
	uint8_t timeout = 4;
	uint8_t maxAttemp = 3;
	bool splitBuffer = true;
	int maxBufferSize = 1400;
	FTcpMessage rbuffer;

	void runContextThread();
	void resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void conn(std::error_code error);
	void package_buffer(const TArray<char>& buffer);
	void write(std::error_code error, std::size_t bytes_sent);
	void read(std::error_code error, std::size_t bytes_recvd);
};
