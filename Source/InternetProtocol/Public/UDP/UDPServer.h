/** 
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "net/asio.h"
#include "udpserver.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateUdpServer);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDelegateUdpServerMessageSent, const FErrorCode &, ErrorCode, int, BytesSent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateUdpServerMessage, const TArray<uint8> &, Buffer, int,  BytesRecv, const FUdpEndpoint &, Endpoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateUdpServerError, const FErrorCode &, ErrorCode);

struct udp_server_t {
	udp_server_t(): socket(context) {
	}

	asio::io_context context;
	udp::socket socket;
	udp::endpoint remote_endpoint;
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|UDP")
class INTERNETPROTOCOL_API UUDPServer : public UObject
{
	GENERATED_BODY()
public:
	UUDPServer() { recv_buffer.SetNumUninitialized(recv_buffer_size); }
	~UUDPServer() {
		if (net.socket.is_open())
			Close();
	}

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
	bool SendTo(const FString &Message, const FUdpEndpoint &Endpoint, const FDelegateUdpServerMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|UDP")
	bool SendBufferTo(const TArray<uint8> &Buffer, const FUdpEndpoint &Endpoint, const FDelegateUdpServerMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|UDP")
	bool Bind(const FServerBindOptions &BindOpts);

	UFUNCTION(BlueprintCallable, Category = "IP|UDP")
	void Close();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateUdpServer OnListening;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateUdpServerMessage OnMessage;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateUdpServer OnClose;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|UDP|Events")
	FDelegateUdpServerError OnError;
private:
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<bool> is_closing = false;
	udp_server_t net;
	asio::error_code error_code;
	size_t recv_buffer_size = 16384;
	TArray<uint8> recv_buffer;

	void run_context_thread();
	void consume_receive_buffer();
	void receive_from_cb(const asio::error_code &error, const size_t bytes_recvd);
};
