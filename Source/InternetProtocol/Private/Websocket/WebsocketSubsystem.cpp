// Fill out your copyright notice in the Description page of Project Settings.


#include "Websocket/WebsocketSubsystem.h"

void UWebsocketSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

UWebsocket* UWebsocketSubsystem::ConstructWebsocketObject()
{
	UWebsocket* WS = NewObject<UWebsocket>();
	return WS;
}
