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
	void OnClientConnected();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="SocketIO")
	void OnClientDisconnected();
	
	UFUNCTION(BlueprintCallable, Category="SocketIO")
	bool Client_Connect(FString url);

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	void Client_Disconect();

	UFUNCTION(BlueprintCallable, Category="SocketIO")
	FString Client_GetSessionID();

	UPROPERTY(BlueprintReadWrite, Category="SocketIO || Server Info")
	FString URL;

private:
	sio::client h;

	void on_connected();
	void on_disconnected();
};
