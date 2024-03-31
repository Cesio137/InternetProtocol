// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "WebSocketsModule.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "Websocket.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateOnConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateOnMessage, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateOnRawMessage, const TArray<uint8>&, Data, int, Size, int, BytesRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateOnConnectionError, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateOnClosed, int32, StatusCode, const FString&, Reason, bool, bWasClean);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateOnMessageSent, const FString&, MessageString);

UCLASS(Blueprintable)
class WEBSOCKET_MMO_API UWebsocket : public UObject
{
	GENERATED_BODY()
	UWebsocket();
	~UWebsocket();
public:
	
	UFUNCTION(BlueprintCallable, Category = "Websockets")
	bool CreateWebsocket(const FString& url, const FString& protocol = "ws");

	UFUNCTION(BlueprintCallable, Category="Websockets")
	void Connect();

	UFUNCTION(BlueprintCallable, Category="Websockets")
	void Close(int32 code = 1000, const FString& reason = "");

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Websockets")
	bool bIsConnected();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Websockets")
	bool bIsWebsocketValid();

	UFUNCTION(BlueprintCallable, Category="Websockets")
	void Reset();

	//Send
	UFUNCTION(BlueprintCallable, Category = "Websockets")
	void Send(const FString& Data);

	UFUNCTION(BlueprintCallable, Category = "Websockets")
	void SendRaw(const TArray<uint8>& Data);
	
	//Events
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Websockets||Events")
	FDelegateOnConnected OnConnected;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Websockets||Events")
	FDelegateOnMessage OnMessage;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Websockets||Events")
	FDelegateOnConnectionError OnConnectionError;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Websockets||Events")
	FDelegateOnClosed OnClosed;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Websockets||Events")
	FDelegateOnMessageSent OnMessageSent;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Websockets||Events")
	FDelegateOnRawMessage OnRawMessage;
	
private:
	TSharedPtr<IWebSocket> ws;
};
