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
DECLARE_DYNAMIC_DELEGATE_OneParam(FDelegateSocketBinaryEvent, TArray<uint8>, BinaryMessage);

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
	
	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void WS_Connect(const FString &url);

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void WS_Disconect();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SocketIO")
	bool WS_IsConnected();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SocketIO")
	FString WS_GetID();

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void WS_EmitStrMessage(const FString& EventName, FString message);

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void WS_EmitRawMessage(const FString& EventName, const TArray<uint8> &BinaryMessage);

	UPROPERTY()
	FDelegateSocketStrEvent SocketStrEvent;

	UPROPERTY()
	FDelegateSocketBinaryEvent SocketBinaryEvent;
	
	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void BindSocketEventStrByName(const FString& EventName, FDelegateSocketStrEvent WebsocketEvent);

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void BindSocketEventRawByName(const FString& EventName, FDelegateSocketBinaryEvent WebsocketEvent);

private:
	sio::client h;
	bool bIsConnected = false;
	void on_connected();
	void on_fail();
	void on_disconnected();

	
};
