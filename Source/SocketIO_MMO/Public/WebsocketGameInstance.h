// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Delegates/DelegateSignatureImpl.inl"
THIRD_PARTY_INCLUDES_START
#include "sio_client.h"
THIRD_PARTY_INCLUDES_END
#include "WebsocketGameInstance.generated.h"

/**
 * 
 */


DECLARE_DYNAMIC_DELEGATE_OneParam(FDelegateSocketStrEvent, FString, Message);
DECLARE_DYNAMIC_DELEGATE_OneParam(FDelegateSocketBoolEvent, bool, Message);
DECLARE_DYNAMIC_DELEGATE_OneParam(FDelegateSocketIntEvent, int, Message);
DECLARE_DYNAMIC_DELEGATE_OneParam(FDelegateSocketFloatEvent, float, Message);

UCLASS()
class SOCKETIO_MMO_API UWebsocketGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	virtual void BeginDestroy() override;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="SocketIO || Event")
	void OnConnected();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="SocketIO || Event")
	void OnFail();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="SocketIO || Event")
	void OnDisconnected();
/*
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="SocketIO || Event")
	void chatMessage(FString message);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="SocketIO || Event")
	void playerName(FString name);
*/
	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void WS_Connect(FString url);

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void WS_Disconect();

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void WS_Emit(FString name, FString msglist);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SocketIO")
	bool WS_IsConnected();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SocketIO")
	FString WS_GetID();

	UPROPERTY()
	FDelegateSocketStrEvent SocketStrEvent;

	UPROPERTY()
	FDelegateSocketBoolEvent SocketBoolEvent;

	UPROPERTY()
	FDelegateSocketIntEvent SocketIntEvent;

	UPROPERTY()
	FDelegateSocketFloatEvent SocketFloatEvent;
	
	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void BindSocketEventStrByName(FString EventName, FDelegateSocketStrEvent WebsocketEvent);

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void BindSocketEventBoolByName(FString EventName, FDelegateSocketBoolEvent WebsocketEvent);

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void BindSocketEventIntByName(FString EventName, FDelegateSocketIntEvent WebsocketEvent);

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void BindSocketEventFloatByName(FString EventName, FDelegateSocketFloatEvent WebsocketEvent);

private:
	sio::client h;
	bool bIsConnected = false;
	void on_connected();
	void on_fail();
	void on_disconnected();

	
};
