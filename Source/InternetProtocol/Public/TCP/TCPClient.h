/** 
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "net/asio.h"
#include "tcpclient.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateTcpClient);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDelegateTcpClientMessageSent, const FErrorCode &, ErrorCode, int, BytesSent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpClientMessage, const TArray<uint8> &, Buffer, int,  BytesRecv);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpClientError, const FErrorCode &, ErrorCode);

struct tcp_client_t {
	tcp_client_t(): socket(context), resolver(context) {
	}

	asio::io_context context;
	tcp::socket socket;
	tcp::endpoint endpoint;
	tcp::resolver resolver;
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|TCP")
class INTERNETPROTOCOL_API UTCPClient : public UObject
{
	GENERATED_BODY()
public:
	UTCPClient() {}
	~UTCPClient() {
		if (net.socket.is_open())
			Close();
		consume_recv_buffer();
	}

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FTcpEndpoint RemoteEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FErrorCode GetErrorCode();

	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	bool Write(const FString &Message, const FDelegateTcpClientMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	bool WriteBuffer(const TArray<uint8> &Buffer, const FDelegateTcpClientMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	bool Connect(const FClientBindOptions &BindOpts);
	
	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	void Close();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpClient OnConnected;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpClientMessage OnMessage;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpClient OnClose;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpClientError OnError;
	
private:
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<bool> is_closing = false;
	tcp_client_t net;
	asio::error_code error_code;
	asio::streambuf recv_buffer;

	void run_context_thread();
	void resolve(const asio::error_code &error, const tcp::resolver::results_type &results);
	void conn(const asio::error_code &error);
	void consume_recv_buffer();
	void read_cb(const asio::error_code &error, const size_t bytes_recvd);
};

struct tcp_client_ssl_t {
	tcp_client_ssl_t(): ssl_context(asio::ssl::context::tlsv13_client),
						ssl_socket(context, ssl_context),
						resolver(context) {
	}

	asio::io_context context;
	asio::ssl::context ssl_context;
	tcp::resolver resolver;
	tcp::endpoint endpoint;
	asio::ssl::stream<tcp::socket> ssl_socket;
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|TCP")
class INTERNETPROTOCOL_API UTCPClientSsl : public UObject
{
	GENERATED_BODY()
public:
	UTCPClientSsl() = default;
	~UTCPClientSsl() {
		if (net.ssl_socket.next_layer().is_open())
			Close();
		consume_recv_buffer();
	}

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

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FTcpEndpoint RemoteEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FErrorCode GetErrorCode();

	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	bool Write(const FString &Message, const FDelegateTcpClientMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	bool WriteBuffer(const TArray<uint8> &Buffer, const FDelegateTcpClientMessageSent &Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	bool Connect(const FClientBindOptions &BindOpts);
	
	UFUNCTION(BlueprintCallable, Category = "IP|TCP")
	void Close();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpClient OnConnected;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpClientMessage OnMessage;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpClient OnClose;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpClientError OnError;
	
private:
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<bool> is_closing = false;
	tcp_client_ssl_t net;
	asio::error_code error_code;
	asio::streambuf recv_buffer;

	void run_context_thread();
	void resolve(const asio::error_code &error, const tcp::resolver::results_type &results);
	void conn(const asio::error_code &error);
	void ssl_handshake(const asio::error_code &error);
	void consume_recv_buffer();
	void read_cb(const asio::error_code &error, const size_t bytes_recvd);
};
