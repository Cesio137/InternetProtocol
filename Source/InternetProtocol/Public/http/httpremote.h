// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "net/asio.h"
#include "httpremote.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateHttpRemote);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDelegateHttpRemoteMessageSent, const FErrorCode&, ErrorCode, int, BytesSent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateHttpRemoteError, const FErrorCode&, ErrorCode);

UCLASS(Blueprintable, BlueprintType, Category = "IP|HTTP")
class INTERNETPROTOCOL_API UHttpRemote : public UObject
{
	GENERATED_BODY()
public:
	UHttpRemote() {}
	~UHttpRemote() {
		if (socket->is_open())
			Close();
	}

	void Construct(asio::io_context &io_context, const uint8 timeout = 0);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	FTcpEndpoint RemoteEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	FErrorCode GetErrorCode();

	tcp::socket &get_socket();

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Headers(const FHttpResponse &Response);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	bool Write(const FDelegateHttpRemoteMessageSent &Callback);

	bool write();

	void connect();
	
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP")
	void Close();

	std::function<void(const FHttpRequest &)> on_request;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpRemote OnClose;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpRemoteError OnError;
	
private:
	FCriticalSection mutex_error;
	TAtomic<bool> is_closing = false;
	TUniquePtr<tcp::socket> socket;
	TUniquePtr<asio::steady_timer> idle_timer;
	uint8 idle_timeout_seconds = 0;
	asio::error_code error_code;
	bool will_close = false;
	FHttpResponse response;
	asio::streambuf recv_buffer;

	void start_idle_timer();
	void reset_idle_timer();
	void write_cb(const asio::error_code &error, const size_t bytes_sent,
					  const FDelegateHttpRemoteMessageSent &callback);
	void write_error_cb(const asio::error_code &error, const size_t bytes_sent);
	void consume_recv_buffer();
	void read_cb(const asio::error_code &error, const size_t bytes_recvd);
	void read_headers(const asio::error_code &error, const std::string &path, const std::string &version);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|HTTP")
class INTERNETPROTOCOL_API UHttpRemoteSsl : public UObject
{
	GENERATED_BODY()
public:
	UHttpRemoteSsl() {}
	~UHttpRemoteSsl() {
		if (ssl_socket->next_layer().is_open())
			Close();
	}

	void Construct(asio::io_context &io_context, asio::ssl::context &ssl_context, const uint8 timeout = 0);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	FTcpEndpoint RemoteEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	FErrorCode GetErrorCode();

	 asio::ssl::stream<tcp::socket> &get_socket();

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Headers(const FHttpResponse &Response);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	bool Write(const FDelegateHttpRemoteMessageSent &Callback);

	bool write();

	void connect();
	
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP")
	void Close();

	std::function<void(const FHttpRequest &)> on_request;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpRemote OnClose;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpRemoteError OnError;
	
private:
	FCriticalSection mutex_error;
	TAtomic<bool> is_closing = false;
	TUniquePtr<asio::ssl::stream<tcp::socket>> ssl_socket;
	TUniquePtr<asio::steady_timer> idle_timer;
	uint8 idle_timeout_seconds = 0;
	asio::error_code error_code;
	bool will_close = false;
	FHttpResponse response;
	asio::streambuf recv_buffer;

	void start_idle_timer();
	void reset_idle_timer();
	void ssl_handshake(const asio::error_code &error);
	void write_cb(const asio::error_code &error, const size_t bytes_sent,
					  const FDelegateHttpRemoteMessageSent &callback);
	void write_error_cb(const asio::error_code &error, const size_t bytes_sent);
	void consume_recv_buffer();
	void read_cb(const asio::error_code &error, const size_t bytes_recvd);
	void read_headers(const asio::error_code &error, const std::string &path, const std::string &version);
};
