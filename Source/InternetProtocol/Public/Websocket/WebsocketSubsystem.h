// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Websocket/Websocket.h"
#include "WebsocketSubsystem.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class INTERNETPROTOCOL_API UWebsocketSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

public:
	UFUNCTION(BlueprintCallable, Category="Websocket||Subsystem")
	UWebsocket* ConstructWebsocketObject();
private:
	
};
