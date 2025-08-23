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
	~UWSClient() {}

	virtual void BeginDestroy() override;

	UFUNCTION(blueprintcallable, Category = "IP|Websocket")
	void AddToRoot();

	UFUNCTION(blueprintcallable, Category = "IP|Websocket")
	void RemoveFromRoot();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	bool IsRooted();

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
	bool is_being_destroyed = false;
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<ECloseState> close_state = ECloseState::CLOSED;
	TAtomic<bool> wait_close_frame_response = true;
	TUniquePtr<asio::steady_timer> idle_timer;
	tcp_client_t net;
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

UCLASS(Blueprintable, BlueprintType, Category = "IP|Websocket")
class INTERNETPROTOCOL_API UWSClientSsl : public UObject
{
	GENERATED_BODY()
public:
	UWSClientSsl() {
		idle_timer = MakeUnique<asio::steady_timer>(net.context);
		Handshake.Path = "/chat";
		Handshake.Headers.Add("Connection", "Upgrade");
		Handshake.Headers.Add("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
		Handshake.Headers.Add("Sec-WebSocket-Version", "13");
		Handshake.Headers.Add("Upgrade", "websocket");
	}
	~UWSClientSsl() {}

	void Construct(const FSecurityContextOpts &SecOpts) {
		file_format_e file_format = SecOpts.File_Format == EFileFormat::asn1 ? file_format_e::asn1 : file_format_e::pem;
		if (!SecOpts.Private_Key.IsEmpty()) {
			const asio::const_buffer buffer(SecOpts.Private_Key.GetCharArray().GetData(), SecOpts.Private_Key.Len());
			net.ssl_context.use_private_key(buffer, file_format);
		}

		if (!SecOpts.Cert.IsEmpty()) {
			const asio::const_buffer buffer(SecOpts.Cert.GetCharArray().GetData(), SecOpts.Cert.Len());
			net.ssl_context.use_certificate(buffer, file_format);
		}

		if (!SecOpts.Cert_Chain.IsEmpty()) {
			const asio::const_buffer buffer(SecOpts.Cert_Chain.GetCharArray().GetData(), SecOpts.Cert_Chain.Len());
			net.ssl_context.use_certificate_chain(buffer);
		}

		if (!SecOpts.RSA_Private_Key.IsEmpty()) {
			const asio::const_buffer buffer(SecOpts.RSA_Private_Key.GetCharArray().GetData(), SecOpts.RSA_Private_Key.Len());
			net.ssl_context.use_rsa_private_key(buffer, file_format);
		}

		if (!SecOpts.Host_Name_Verification.IsEmpty()) {
			net.ssl_context.set_verify_callback(asio::ssl::host_name_verification(TCHAR_TO_UTF8(*SecOpts.Host_Name_Verification)));
		}

		switch (SecOpts.Verify_Mode) {
		case EVerifyMode::None:
			net.ssl_context.set_verify_mode(asio::ssl::verify_none);
			break;
		case EVerifyMode::Verify_Peer:
			net.ssl_context.set_verify_mode(asio::ssl::verify_peer);
			break;
		case EVerifyMode::Verify_Fail_If_No_Peer_Cert:
			net.ssl_context.set_verify_mode(asio::ssl::verify_fail_if_no_peer_cert);
			break;
		case EVerifyMode::Verify_Client_Once:
			net.ssl_context.set_verify_mode(asio::ssl::verify_client_once);
			break;
		default:
			break;
		}

		net.ssl_socket = asio::ssl::stream<tcp::socket>(net.context, net.ssl_context);
	}

	virtual void BeginDestroy() override;

	UFUNCTION(blueprintcallable, Category = "IP|Websocket")
	void AddToRoot();

	UFUNCTION(blueprintcallable, Category = "IP|Websocket")
	void RemoveFromRoot();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	bool IsRooted();

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
	bool is_being_destroyed = false;
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<ECloseState> close_state = ECloseState::CLOSED;
	TAtomic<bool> wait_close_frame_response = true;
	TUniquePtr<asio::steady_timer> idle_timer;
	tcp_client_ssl_t net;
	asio::error_code error_code;
	asio::streambuf recv_buffer;

	void start_idle_timer();
	void send_close_frame(const uint16_t code, const FString &reason, bool wait_server = false);
	void close_frame_sent_cb(const asio::error_code& error, const size_t bytes_sent, const uint16_t code, const FString& reason);
	void run_context_thread();
	void resolve(const asio::error_code &error, const tcp::resolver::results_type &results);
	void conn(const asio::error_code &error);
	void ssl_handshake(const asio::error_code &error);
	void write_handshake_cb(const asio::error_code &error, const size_t bytes_sent);
	void read_handshake_cb(const asio::error_code &error, const size_t bytes_recvd);
	void read_headers(const asio::error_code &error, FHttpResponse &response);
	void consume_recv_buffer();
	void read_cb(const asio::error_code &error, const size_t bytes_recvd);
};