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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpRetry, const int, attemp);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpMessageSent, const int, size);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpMessageReceived, const int, size, const FTcpMessage, message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpError, const int, code, const FString&, exception);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UTCPClient : public UObject
{
	GENERATED_BODY()

public:
	UTCPClient()
	{
	}

	~UTCPClient()
	{
		tcp.context.stop();
		pool->stop();
		consume_response_buffer();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Remote")
	void setHost(const FString& ip, const FString& port)
	{
		host = ip;
		service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Local")
	FString getLocalAdress() const {
		if (isConnected())
			return UTF8_TO_TCHAR(tcp.socket.local_endpoint().address().to_string().c_str());
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Local")
	FString getLocalPort() const {
		if (isConnected())
			return FString::FromInt(tcp.socket.local_endpoint().port());
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Local")
	FString getRemoteAdress() const {
		if (isConnected())
			return UTF8_TO_TCHAR(tcp.socket.remote_endpoint().address().to_string().c_str());
		return host;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Local")
	FString getRemovePort() const {
		if (isConnected())
			return FString::FromInt(tcp.socket.remote_endpoint().port());
		return service;
	}

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setTimeout(uint8 value = 3) { timeout = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	uint8 getTimeout() const { return timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setMaxAttemp(uint8 value = 3) { maxAttemp = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	uint8 getMaxAttemp() const { return timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setMaxBufferSize(int value = 1400) { maxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	int getMaxBufferSize() const { return maxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setSplitPackage(bool value = true) { splitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	bool getSplitPackage() const { return splitBuffer; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool send(const FString& message);
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool sendRaw(const TArray<uint8>& buffer);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	bool connect();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Connection")
	bool isConnected() const { return tcp.socket.is_open(); }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	void close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Error")
	int getErrorCode() const { return tcp.error_code.value(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Error")
	FString getErrorMessage() const { return tcp.error_code.message().data(); }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpConnection OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpRetry OnConnectionWillRetry;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpConnection OnConnectionClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpError OnError;

private:
	TUniquePtr<asio::thread_pool> pool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	std::mutex mutexIO;
	std::mutex mutexBuffer;
	FAsioTcp tcp;
	FString host = "localhost";
	FString service;
	uint8_t timeout = 3;
	uint8_t maxAttemp = 3;
	bool splitBuffer = true;
	int maxSendBufferSize = 1400;
	asio::streambuf response_buffer;

	void package_string(const FString& str);
	void package_buffer(const TArray<uint8>& buffer);

	void consume_response_buffer()
	{
		response_buffer.consume(response_buffer.size());
	}

	void run_context_thread();
	void resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void conn(const std::error_code& error);
	void write(const std::error_code& error, const size_t bytes_sent);
	void read(const std::error_code& error, const size_t bytes_recvd);
};
