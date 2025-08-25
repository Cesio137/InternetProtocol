/** 
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "net/asio.h"
#include "wsremote.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateWsRemote);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateWsRemoteHandshake, const FHttpRequest&, Handshake);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDelegateWsRemoteMessageSent, const FErrorCode &, ErrorCode, int, BytesSent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateWsRemoteMessage, const TArray<uint8> &, Buffer, bool, bIsBinary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateWsRemoteClose, int, Code, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateWsRemoteError, const FErrorCode&, ErrorCode);

UCLASS(Blueprintable, BlueprintType, Category = "IP|Websocket")
class INTERNETPROTOCOL_API UWSRemote : public UObject
{
	GENERATED_BODY()
public:
	UWSRemote() {
		handshake.Status_Code = 101;
		handshake.Status_Message = "Switching Protocols";
		handshake.Headers.Add("Upgrade", "websocket");
		handshake.Headers.Add("Connection", "Upgrade");
		handshake.Headers.Add("Sec-WebSocket-Accept", "");
	}
	~UWSRemote() {}

	virtual void BeginDestroy() override;

	void Construct(asio::io_context &io_context);

	void Destroy();

	UFUNCTION(bLueprintCallable, BlueprintPure, Category = "IP|Websocket")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	FTcpEndpoint RemoteEndpoint();

	tcp::socket &get_socket();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	FErrorCode GetErrorCode();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool Write(const FString &Message, const FDataframe &Dataframe, const FDelegateWsRemoteMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool WriteBuffer(const TArray<uint8> &Buffer, const FDataframe &Dataframe, const FDelegateWsRemoteMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool Ping(const FDelegateWsRemoteMessageSent &Callback);

	bool ping();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool Pong(const FDelegateWsRemoteMessageSent &Callback);

	bool pong();

	void connect();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	void End(int code, const FString &reason);

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	void Close(int code, const FString &reason);

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemoteHandshake OnConnected;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemoteHandshake OnUnexpectedHandshake;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemoteMessage OnMessage;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemote OnPing;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemote OnPong;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemoteClose OnClose;

	std::function<void()> on_close;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemoteError OnError;
	
private:
	bool is_being_destroyed = false;
	FCriticalSection mutex_error;
	TAtomic<ECloseState> close_state = ECloseState::CLOSED;
	TAtomic<bool> wait_close_frame_response = true;
	TUniquePtr<asio::steady_timer> idle_timer;
	TUniquePtr<tcp::socket> socket;
	asio::error_code error_code;
	FHttpResponse handshake;
	asio::streambuf recv_buffer;

	void start_idle_timer();
	void send_close_frame(const uint16_t code, const FString &reason, bool wait_server = false);
	void close_frame_sent_cb(const asio::error_code& error, const size_t bytes_sent, const uint16_t code, const FString& reason);
	void read_handshake_cb(const asio::error_code &error, const size_t bytes_recvd);
	void read_headers(const asio::error_code &error, FHttpRequest &request);
	void write_handshake_cb(const asio::error_code &error, const size_t bytes_sent, const FHttpRequest &request);
	void consume_recv_buffer();
	void read_cb(const asio::error_code &error, const size_t bytes_recvd);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|Websocket")
class INTERNETPROTOCOL_API UWSRemoteSsl : public UObject
{
	GENERATED_BODY()
public:
	UWSRemoteSsl() {
		handshake.Status_Code = 101;
		handshake.Status_Message = "Switching Protocols";
		handshake.Headers.Add("Upgrade", "websocket");
		handshake.Headers.Add("Connection", "Upgrade");
		handshake.Headers.Add("Sec-WebSocket-Accept", "");
	}
	~UWSRemoteSsl() {}

	virtual void BeginDestroy() override;

	void Construct(asio::io_context &io_context, asio::ssl::context &ssl_context);

	void Destroy();

	UFUNCTION(bLueprintCallable, BlueprintPure, Category = "IP|Websocket")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	FTcpEndpoint RemoteEndpoint();

	asio::ssl::stream<tcp::socket> &get_socket();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	FErrorCode GetErrorCode();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool Write(const FString &Message, const FDataframe &Dataframe, const FDelegateWsRemoteMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool WriteBuffer(const TArray<uint8> &Buffer, const FDataframe &Dataframe, const FDelegateWsRemoteMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool Ping(const FDelegateWsRemoteMessageSent &Callback);

	bool ping();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool Pong(const FDelegateWsRemoteMessageSent &Callback);

	bool pong();

	void connect();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	void End(int code, const FString &reason);

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	void Close(int code, const FString &reason);

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemoteHandshake OnConnected;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemoteHandshake OnUnexpectedHandshake;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemoteMessage OnMessage;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemote OnPing;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemote OnPong;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemoteClose OnClose;

	std::function<void()> on_close;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsRemoteError OnError;
	
private:
	bool is_being_destroyed = false;
	FCriticalSection mutex_error;
	TAtomic<ECloseState> close_state = ECloseState::CLOSED;
	TAtomic<bool> wait_close_frame_response = true;
	TUniquePtr<asio::steady_timer> idle_timer;
	TUniquePtr<asio::ssl::stream<tcp::socket>> ssl_socket;
	asio::error_code error_code;
	FHttpResponse handshake;
	asio::streambuf recv_buffer;

	void start_idle_timer();
	void send_close_frame(const uint16_t code, const FString &reason, bool wait_server = false);
	void close_frame_sent_cb(const asio::error_code& error, const size_t bytes_sent, const uint16_t code, const FString& reason);
	void ssl_handshake(const asio::error_code &error);
	void read_handshake_cb(const asio::error_code &error, const size_t bytes_recvd);
	void read_headers(const asio::error_code &error, FHttpRequest &request);
	void write_handshake_cb(const asio::error_code &error, const size_t bytes_sent, const FHttpRequest &request);
	void consume_recv_buffer();
	void read_cb(const asio::error_code &error, const size_t bytes_recvd);
};
