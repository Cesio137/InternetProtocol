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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpMessageSent, int, size);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateUdpMessageReceived, int, size, const FUdpMessage, message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateUdpError, int, code, const FString&, exception);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UUDPClient : public UObject
{
	GENERATED_BODY()
public:
	UUDPClient()
	{
		rbuffer.message.SetNumUninitialized(1024);
	}

	~UUDPClient()
	{
		
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

	/*THREADS*/
	UFUNCTION(BlueprintCallable, Category = "IP||UDP||Threads")
	void setThreadNumber(int value = 2) { pool = MakeUnique<asio::thread_pool>(value); }

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
	TUniquePtr<asio::thread_pool> pool = MakeUnique<asio::thread_pool>(2);
	std::mutex mutexIO;
	std::mutex mutexBuffer;
	FAsioUdp udp;
	FString host = "localhost";
	FString service;
	FUdpMessage rbuffer;

	void runContextThread();
	void resolve(std::error_code error, asio::ip::udp::resolver::results_type results);
	void conn(std::error_code error);
	void package_buffer(const TArray<char>& buffer);
	void send_to(std::error_code error, std::size_t bytes_sent);
	void receive_from(std::error_code error, std::size_t bytes_recvd);
};
