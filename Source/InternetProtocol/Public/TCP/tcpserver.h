/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "tcp/tcpremote.h"
#include "tcpserver.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateTcpServer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpServerClientConnect, UTCPRemote*, Client);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpServerClientSslConnect, UTCPRemoteSsl*, Client);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpServerError, const FErrorCode &, ErrorCode);

struct tcp_server_t {
	tcp_server_t(): acceptor(context) {
	}

	asio::io_context context;
	tcp::acceptor acceptor;
	TSet<UTCPRemote*> clients;
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|TCP")
class INTERNETPROTOCOL_API UTCPServer : public UObject
{
	GENERATED_BODY()
public:

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	const TSet<UTCPRemote*> &Clients();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FErrorCode GetErrorCode();

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void SetMaxConnections(int Val);

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	int GetMaxConnections();

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	bool Open(const FServerBindOptions &BindOpts);

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void Close();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpServer OnListening;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpServerClientConnect OnClientAccepted;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpServer OnClose;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpServerError OnError;

private:
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<bool> is_closing = false;
	tcp_server_t net;
	asio::error_code error_code;
	int max_connections = 2147483647;

	void run_context_thread();
	void accept(const asio::error_code &error, UTCPRemote* client);
};

struct tcp_server_ssl_t {
	tcp_server_ssl_t(): acceptor(context), ssl_context(asio::ssl::context::tlsv13) {
	}

	asio::io_context context;
	asio::ssl::context ssl_context;
	tcp::acceptor acceptor;
	TSet<UTCPRemoteSsl*> ssl_clients;
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|TCP")
class INTERNETPROTOCOL_API UTCPServerSsl : public UObject
{
	GENERATED_BODY()
public:
	UTCPServerSsl() {}
	~UTCPServerSsl() {
		if (net.acceptor.is_open())
			Close();
	}

	void InitializeSsl(const FSecurityContextOpts &SecOpts) {
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
	}

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	const TSet<UTCPRemoteSsl*> &Clients();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FErrorCode GetErrorCode();

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void SetMaxConnections(int Val);

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	int GetMaxConnections();

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	bool Open(const FServerBindOptions &BindOpts);

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void Close();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpServer OnListening;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpServerClientSslConnect OnClientAccepted;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpServer OnClose;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateTcpServerError OnError;

private:
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<bool> is_closing = false;
	tcp_server_ssl_t net;
	asio::error_code error_code;
	int max_connections = 2147483647;

	void run_context_thread();
	void accept(const asio::error_code &error, UTCPRemoteSsl* client);
};