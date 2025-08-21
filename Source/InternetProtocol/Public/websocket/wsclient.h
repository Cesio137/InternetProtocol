/** 
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "net/asio.h"
#include "wsclient.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateWsClient);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateWsClientHandshake, const FHttpResponse&, Handshake);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDelegateWsClientMessageSent, const FErrorCode &, ErrorCode, int, BytesSent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateWsClientMessage, const TArray<uint8> &, Buffer, bool, bIsBinary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateWsClientClose, int, Code, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateWsClientError, const FErrorCode&, ErrorCode);

struct ws_client_t {
	ws_client_t() : socket(context), resolver(context) {
	}

	asio::io_context context;
	tcp::socket socket;
	tcp::endpoint endpoint;
	tcp::resolver resolver;
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|Websocket")
class INTERNETPROTOCOL_API UWSClient : public UObject
{
	GENERATED_BODY()
public:
	UWSClient() {
		idle_timer = MakeUnique<asio::steady_timer>(net.context);
		Handshake.Path = "/chat";
		Handshake.Headers.Add("Connection", "Upgrade");
		Handshake.Headers.Add("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
		Handshake.Headers.Add("Sec-WebSocket-Version", "13");
		Handshake.Headers.Add("Upgrade", "websocket");
	}
	~UWSClient() {
		UE_LOG(LogTemp, Warning, TEXT("UWSClient::~UWSClient()"));
		if (net.socket.is_open())
			Close(1000, "Shutdown");
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "IP|Websocket")
	FHttpRequest Handshake;

	UFUNCTION(bLueprintCallable, BlueprintPure, Category = "IP|Websocket")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	FTcpEndpoint RemoteEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	FErrorCode GetErrorCode();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool Write(const FString &Message, const FDataframe &Dataframe, const FDelegateWsClientMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool WriteBuffer(const TArray<uint8> &Buffer, const FDataframe &Dataframe, const FDelegateWsClientMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool Ping(const FDelegateWsClientMessageSent &Callback);

	bool ping();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool Pong(const FDelegateWsClientMessageSent &Callback);

	bool pong();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	bool Connect(const FClientBindOptions &BindOpts);

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	void End(int code, const FString &reason);

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket")
	void Close(int code, const FString &reason);

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsClientHandshake OnConnected;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsClientHandshake OnUnexpectedHandshake;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsClientMessage OnMessage;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsClient OnPing;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsClient OnPong;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsClientClose OnClose;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsClientError OnError;
	
private:
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<ECloseState> close_state = ECloseState::CLOSED;
	TAtomic<bool> wait_close_frame_response = true;
	TUniquePtr<asio::steady_timer> idle_timer;
	ws_client_t net;
	asio::error_code error_code;
	asio::streambuf recv_buffer;

	void start_idle_timer();
	void send_close_frame(const uint16_t code, const FString &reason, bool wait_server = false);
	void close_frame_sent_cb(const asio::error_code& error, const size_t bytes_sent, const uint16_t code, const FString& reason);
	void run_context_thread();
	void resolve(const asio::error_code &error, const tcp::resolver::results_type &results);
	void conn(const asio::error_code &error);
	void write_handshake_cb(const asio::error_code &error, const size_t bytes_sent);
	void read_handshake_cb(const asio::error_code &error, const size_t bytes_recvd);
	void read_headers(const asio::error_code &error, FHttpResponse &response);
	void consume_recv_buffer();
	void read_cb(const asio::error_code &error, const size_t bytes_recvd);
};
