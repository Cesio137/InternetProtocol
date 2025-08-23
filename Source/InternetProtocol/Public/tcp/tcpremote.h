/** 
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "net/asio.h"
#include "Templates/PimplPtr.h"
#include "tcpremote.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateTcpRemote);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDelegateTcpRemoteMessageSent, const FErrorCode &, ErrorCode, int, BytesSent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpRemoteMessage, const TArray<uint8> &, Buffer, int,  BytesRecv);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpRemoteError, const FErrorCode &, ErrorCode);

UCLASS(BlueprintType, Category = "IP|TCP")
class INTERNETPROTOCOL_API UTCPRemote : public UObject
{
	GENERATED_BODY()
public:
	UTCPRemote() = default;
	~UTCPRemote() = default;
	
	virtual void BeginDestroy() override;

	void Construct(asio::io_context &io_context);

	void Destroy();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FTcpEndpoint RemoteEndpoint();

	tcp::socket &get_socket();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FErrorCode GetErrorCode();

	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	bool Write(const FString &Message, const FDelegateTcpRemoteMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	bool WriteBuffer(const TArray<uint8> &Buffer, const FDelegateTcpRemoteMessageSent &Callback);

	void connect();
	
	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	void Close();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpRemote OnConnected;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpRemoteMessage OnMessage;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpRemote OnClose;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpRemoteError OnError;

	std::function<void()> on_close;

private:
	bool is_being_destroyed = false;
	FCriticalSection mutex_error;
	TUniquePtr<tcp::socket> socket;
	asio::error_code error_code;
	asio::streambuf recv_buffer;

	void consume_recv_buffer();
	void read_cb(const asio::error_code &error, std::size_t bytes_recvd);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|TCP")
class INTERNETPROTOCOL_API UTCPRemoteSsl : public UObject
{
	GENERATED_BODY()
public:
	UTCPRemoteSsl() {}

	virtual void BeginDestroy() override;

	void Construct(asio::io_context &io_context, asio::ssl::context &ssl_context);

	void Destroy();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FTcpEndpoint RemoteEndpoint();

	asio::ssl::stream<tcp::socket>& get_socket();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FErrorCode GetErrorCode();

	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	bool Write(const FString &Message, const FDelegateTcpRemoteMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	bool WriteBuffer(const TArray<uint8> &Buffer, const FDelegateTcpRemoteMessageSent &Callback);

	void connect();
	
	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	void Close();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpRemote OnConnected;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpRemoteMessage OnMessage;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpRemote OnClose;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpRemoteError OnError;

	std::function<void()> on_close;

private:
	bool is_being_destroyed = false;
	FCriticalSection mutex_error;
	TUniquePtr<asio::ssl::stream<tcp::socket>> ssl_socket;
	asio::error_code error_code;
	asio::streambuf recv_buffer;

	void ssl_handshake(const asio::error_code &error);
	void consume_recv_buffer();
	void read_cb(const asio::error_code &error, std::size_t bytes_recvd);
};
