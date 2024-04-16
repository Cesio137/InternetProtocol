// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "IWebSocket.h"
#include "WebSocketsModule.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "Library/EnumLibrary.h"
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

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UWebsocket : public UObject
{
	GENERATED_BODY()
protected:
	virtual void BeginDestroy() override;
	
public:
	UFUNCTION(BlueprintCallable, Category = "Websocket", meta=(ExpandEnumAsExecs = "Output"))
	void ConstructWebsocket(TEnumAsByte<EOutputExecPins>& Output, const FString& url, const FString& protocol = "ws");

	UFUNCTION(BlueprintCallable, Category="Websocket")
	void Connect();

	UFUNCTION(BlueprintCallable, Category="Websocket")
	void Close(int32 code = 1000, const FString& reason = "");

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Websocket")
	bool bIsConnected();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Websocket")
	bool bIsWebsocketValid();

	UFUNCTION(BlueprintCallable, Category="Websocket")
	void Reset();

	//Send
	UFUNCTION(BlueprintCallable, Category = "Websocket")
	void Send(const FString& Data);

	UFUNCTION(BlueprintCallable, Category = "Websocket")
	void SendRaw(const TArray<uint8>& Data);
	
	//Events
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Websocket||Events")
	FDelegateOnConnected OnConnected;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Websocket||Events")
	FDelegateOnMessage OnMessage;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Websocket||Events")
	FDelegateOnConnectionError OnConnectionError;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Websocket||Events")
	FDelegateOnClosed OnClosed;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Websocket||Events")
	FDelegateOnMessageSent OnMessageSent;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Websocket||Events")
	FDelegateOnRawMessage OnRawMessage;

private:
	TSharedPtr<IWebSocket> websocket;
};
