// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "http/httpremote.h"
#include "httpserver.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegatHttpServer);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDelegateHttpServerRequest, const FHttpRequest&, Request, UHttpRemote*, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateHttpServerError, const FErrorCode &, ErrorCode);

struct http_server_t {
	http_server_t(): acceptor(context) {
	}

	asio::io_context context;
	tcp::acceptor acceptor;
	TSet<UHttpRemote*> clients;
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|HTTP")
class INTERNETPROTOCOL_API UHttpServer : public UObject
{
	GENERATED_BODY()
public:

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	TSet<UHttpRemote*> &Clients();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	FErrorCode GetErrorCode();

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void SetMaxConnections(int Val);

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	int GetMaxConnections();

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void SetIdleTimeout(uint8 Val);

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|TCP")
	uint8 GetIdleTimeout();

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void All(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void Get(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void Post(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void Put(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void Del(const FString &Path, const FDelegateHttpServerRequest &Callback);
	
	UFUNCTION(blueprintcallable, Category = "IP|TCP")
    void Head(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void Options(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void Patch(const FString &Path, const FDelegateHttpServerRequest &Callback);

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	bool Open(const FServerBindOptions &BindOpts);

	UFUNCTION(blueprintcallable, Category = "IP|TCP")
	void Close();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegatHttpServer OnClose;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|TCP|Events")
	FDelegateHttpServerError OnError;
	
private:
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<bool> is_closing = false;
	http_server_t net;
	asio::error_code error_code;
	int max_connections = 2147483647;
	uint8 iddle_timeout = 0;

	TMap<FString, FDelegateHttpServerRequest> all_cb;
	TMap<FString, FDelegateHttpServerRequest> get_cb;
	TMap<FString, FDelegateHttpServerRequest> post_cb;
	TMap<FString, FDelegateHttpServerRequest> put_cb;
	TMap<FString, FDelegateHttpServerRequest> del_cb;
	TMap<FString, FDelegateHttpServerRequest> head_cb;
	TMap<FString, FDelegateHttpServerRequest> options_cb;
	TMap<FString, FDelegateHttpServerRequest> patch_cb;

	void run_context_thread();
	void accept(const asio::error_code &error, UHttpRemote* client);
	void read_cb(const FHttpRequest &request, UHttpRemote* client);
};
