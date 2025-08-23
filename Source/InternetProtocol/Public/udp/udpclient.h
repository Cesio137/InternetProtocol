/** 
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "net/asio.h"
#include "udpclient.generated.h"

using namespace asio::ip;

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateUdpClient);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDelegateUdpClientMessageSent, const FErrorCode &, ErrorCode, int, BytesSent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateUdpClientMessage, const TArray<uint8> &, Buffer, int,  BytesRecv);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpClientError, const FErrorCode &, ErrorCode);

UCLASS(Blueprintable, BlueprintType, Category = "IP|UDP")
class INTERNETPROTOCOL_API UUDPClient: public UObject
{
	GENERATED_BODY()
public:
	UUDPClient() {
		recv_buffer.SetNumUninitialized(recv_buffer_size);
	};
	~UUDPClient() {}

	virtual void BeginDestroy() override;

	UFUNCTION(blueprintcallable, Category = "IP|UDP")
	void AddToRoot();

	UFUNCTION(blueprintcallable, Category = "IP|UDP")
	void RemoveFromRoot();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|UDP")
	bool IsRooted();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|UDP")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|UDP")
	FUdpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|UDP")
	FUdpEndpoint RemoteEndpoint();

	UFUNCTION(BlueprintCallable, Category = "IP|UDP")
	void SetRecvBufferSize(int Val);

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|UDP")
	int GetRecvBufferSize();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|UDP")
	FErrorCode GetErrorCode();

	UFUNCTION(BlueprintCallable, Category = "IP|UDP")
	bool Send(const FString &Message, const FDelegateUdpClientMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|UDP")
	bool SendBuffer(const TArray<uint8> &Buffer, const FDelegateUdpClientMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|UDP")
	bool Connect(const FClientBindOptions &BindOpts);

	UFUNCTION(BlueprintCallable, Category = "IP|UDP")
	void Close();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateUdpClient OnConnected;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateUdpClientMessage OnMessage;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateUdpClient OnClose;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateUdpClientError OnError;

private:
	bool is_being_destroyed = false;
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<bool> is_closing = false;
	udp_client_t net;
	asio::error_code error_code;
	size_t recv_buffer_size = 16384;
	TArray<uint8> recv_buffer;

	void run_context_thread();
	void resolve(const asio::error_code &error, const udp::resolver::results_type &results);
	void conn(const asio::error_code &error);
	void consume_recv_buffer();
	void receive_from_cb(const asio::error_code &error, const size_t bytes_recvd);
};
