// Fill out your copyright notice in the Description page of Project Settings.


#include "WebsocketGameInstance.h"

void UWebsocketGameInstance::Init()
{
	Super::Init();
	h.set_open_listener(std::bind(&UWebsocketGameInstance::on_connected, this));
	h.set_close_listener(std::bind(&UWebsocketGameInstance::on_disconnected, this));
}

bool UWebsocketGameInstance::Client_Connect(FString url)
{
	h.connect(std::string(TCHAR_TO_UTF8(*url)));
	return h.opened();
}

void UWebsocketGameInstance::Client_Disconect()
{
	h.close();
}

void UWebsocketGameInstance::Client_Emit(FString name, FString msglist)
{
	h.socket()->emit(TCHAR_TO_UTF8(*name), std::string(TCHAR_TO_UTF8(*msglist)));
}

void UWebsocketGameInstance::on_connected()
{
	OnClientConnected();
}

void UWebsocketGameInstance::on_disconnected()
{
	OnClientDisconnected();
}
