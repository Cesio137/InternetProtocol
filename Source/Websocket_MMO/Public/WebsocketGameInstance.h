// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "IWebSocket.h"
#include "WebSocketsModule.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "WebsocketGameInstance.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_DELEGATE_OneParam(FDelegateWebsocketEvent, FString, Message);

UCLASS()
class WEBSOCKET_MMO_API UWebsocketGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:

	virtual void Shutdown() override;

	UFUNCTION(BlueprintCallable, Category = "Websockets")
	void WS_CreateWebsocket(const FString& url, const FString& protocol = "ws");

	UFUNCTION(BlueprintCallable, Category="Websockets")
	void WS_Connect();
	
	UFUNCTION(BlueprintCallable, Category="Websockets")
	bool WS_IsConnected();

	UFUNCTION(BlueprintCallable, Category="Websockets")
	void WS_Close(int32 code = 1000, const FString& reason = "");

	UFUNCTION(BlueprintCallable, Category = "Websockets")
	void BindSocketEventByName(const FDelegateWebsocketEvent& Event);

	UFUNCTION(BlueprintCallable, Category="Websockets")
	void WS_EmitString(const FString& EventName, const FString& message);

	UFUNCTION(BlueprintCallable, Category="Websockets")
	void WS_EmitRaw(const FString& EventName, const TArray<uint8>& data);

	UFUNCTION(BlueprintImplementableEvent, Category="Websockets||Events")
	void WS_OnConnected();

	UFUNCTION(BlueprintImplementableEvent, Category="Websockets||Events")
	void WS_OnConnectionError(const FString & Error);

	UFUNCTION(BlueprintImplementableEvent, Category="Websockets||Events")
	void WS_OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);

	UFUNCTION(BlueprintImplementableEvent, Category="Websockets||Events")
	void WS_OnMessage(const FString & Message);

	UFUNCTION(BlueprintImplementableEvent, Category="Websockets||Events")
	void WS_OnMessageSent(const FString& MessageString);

private:
	TSharedPtr<IWebSocket> ws;
	
};
