/** 
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "net/asio.h"
#include "wsserver.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateWsServer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateWsServerClientConnect, UWSRemote*, Client);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateWsServerClientSslConnect, UWSRemoteSsl*, Client);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateWsServerError, const FErrorCode &, ErrorCode);

UCLASS(Blueprintable, BlueprintType, Category = "IP|Websocket")
class INTERNETPROTOCOL_API UWSServer : public UObject
{
	GENERATED_BODY()
public:
	UWSServer() {}
	~UWSServer() {}
	
	virtual void BeginDestroy() override;

	UFUNCTION(blueprintcallable, Category = "IP|Websocket")
	void AddToRoot();

	UFUNCTION(blueprintcallable, Category = "IP|Websocket")
	void RemoveFromRoot();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	bool IsRooted();
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "IP|Websocket")
	int Backlog = 2147483647;

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	bool IsOpen();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	FTcpEndpoint LocalEndpoint();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	const TSet<UWSRemote*> &Clients();

	UFUNCTION(blueprintcallable, BlueprintPure, Category = "IP|Websocket")
	FErrorCode GetErrorCode();

	UFUNCTION(blueprintcallable, Category = "IP|Websocket")
	bool Open(const FServerBindOptions &BindOpts);

	UFUNCTION(blueprintcallable, Category = "IP|Websocket")
	void Close();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsServer OnListening;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsServerClientConnect OnClientAccepted;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsServer OnClose;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsServerError OnError;

private:
	bool is_being_destroyed = false;
	FCriticalSection mutex_io;
	FCriticalSection mutex_error;
	TAtomic<bool> is_closing = false;
	tcp_server_t<UWSRemote> net;
	asio::error_code error_code;

	void run_context_thread();
	void accept(const asio::error_code &error, UWSRemote* remote);
};
