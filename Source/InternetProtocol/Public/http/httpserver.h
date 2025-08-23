﻿/** 
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "http/httpremote.h"
#include "httpserver.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegatHttpServer);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDelegateHttpServerRequest, const FHttpRequest&, Request, UHttpRemote*, Response);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDelegateHttpServerRequestSsl, const FHttpRequest&, Request, UHttpRemoteSsl*, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateHttpServerError, const FErrorCode &, ErrorCode);

UCLASS(Blueprintable, BlueprintType, Category = "IP|HTTP")
class INTERNETPROTOCOL_API UHttpServer : public UObject
{
	GENERATED_BODY()
public:
	UHttpServer() {}
	~UHttpServer() {}

	virtual void BeginDestroy() override;

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void AddToRoot();

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void RemoveFromRoot();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	bool IsRooted();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "IP|HTTP")
	int Backlog = 2147483647;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "IP|HTTP")
	uint8 IdleTimeoutSeconds = 0;

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	TSet<UHttpRemote*> &Clients();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	FErrorCode GetErrorCode();

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void All(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Get(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Post(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Put(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Del(const FString &Path, const FDelegateHttpServerRequest &Callback);
	
	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
    void Head(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Options(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Patch(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	bool Open(const FServerBindOptions &BindOpts);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Close();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegatHttpServer OnClose;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpServerError OnError;
	
private:
	bool is_being_destroyed = false;
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<bool> is_closing = false;
	tcp_server_t<UHttpRemote> net;
	asio::error_code error_code;

	TMap<FString, FDelegateHttpServerRequest> all_cb;
	TMap<FString, FDelegateHttpServerRequest> get_cb;
	TMap<FString, FDelegateHttpServerRequest> post_cb;
	TMap<FString, FDelegateHttpServerRequest> put_cb;
	TMap<FString, FDelegateHttpServerRequest> del_cb;
	TMap<FString, FDelegateHttpServerRequest> head_cb;
	TMap<FString, FDelegateHttpServerRequest> options_cb;
	TMap<FString, FDelegateHttpServerRequest> patch_cb;

	void run_context_thread();
	void accept(const asio::error_code &error, UHttpRemote* remote);
	void read_cb(const FHttpRequest &request, UHttpRemote* remote);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|HTTP")
class INTERNETPROTOCOL_API UHttpServerSsl : public UObject
{
	GENERATED_BODY()
public:
	UHttpServerSsl() {}
	~UHttpServerSsl() {}

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
	}

	virtual void BeginDestroy() override;

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void AddToRoot();

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void RemoveFromRoot();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	bool IsRooted();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "IP|HTTP")
	int Backlog = 2147483647;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "IP|HTTP")
	uint8 IdleTimeoutSeconds = 0;

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	TSet<UHttpRemoteSsl*> &Clients();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|HTTP")
	FErrorCode GetErrorCode();

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void All(const FString &Path, const FDelegateHttpServerRequestSsl &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Get(const FString &Path, const FDelegateHttpServerRequestSsl &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Post(const FString &Path, const FDelegateHttpServerRequestSsl &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Put(const FString &Path, const FDelegateHttpServerRequestSsl &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Del(const FString &Path, const FDelegateHttpServerRequestSsl &Callback);
	
	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
    void Head(const FString &Path, const FDelegateHttpServerRequestSsl &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Options(const FString &Path, const FDelegateHttpServerRequestSsl &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Patch(const FString &Path, const FDelegateHttpServerRequestSsl &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	bool Open(const FServerBindOptions &BindOpts);

	UFUNCTION(blueprintcallable, Category = "IP|HTTP")
	void Close();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegatHttpServer OnClose;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpServerError OnError;
	
private:
	bool is_being_destroyed = false;
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<bool> is_closing = false;
	tcp_server_ssl_t<UHttpRemoteSsl> net;
	asio::error_code error_code;

	TMap<FString, FDelegateHttpServerRequestSsl> all_cb;
	TMap<FString, FDelegateHttpServerRequestSsl> get_cb;
	TMap<FString, FDelegateHttpServerRequestSsl> post_cb;
	TMap<FString, FDelegateHttpServerRequestSsl> put_cb;
	TMap<FString, FDelegateHttpServerRequestSsl> del_cb;
	TMap<FString, FDelegateHttpServerRequestSsl> head_cb;
	TMap<FString, FDelegateHttpServerRequestSsl> options_cb;
	TMap<FString, FDelegateHttpServerRequestSsl> patch_cb;

	void run_context_thread();
	void accept(const asio::error_code &error, UHttpRemoteSsl* remote);
	void read_cb(const FHttpRequest &request, UHttpRemoteSsl* remote);
};
