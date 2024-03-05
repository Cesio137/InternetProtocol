// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "sio_client.h"
#include "WebsocketGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class SOCKETIO_MMO_API UWebsocketGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="SocketIO")
	void OnConnected();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="SocketIO")
	void OnFail();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="SocketIO")
	void OnDisconnected();
	
	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void WS_Connect(FString url);

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void WS_Disconect();

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void WS_Emit(FString name, FString msglist);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SocketIO")
	bool WS_IsConnected();

private:
	sio::client h;
	bool IsSioConnected = false;
	void on_connected();
	void on_fail();
	void on_disconnected();
};
