// Fill out your copyright notice in the Description page of Project Settings.


#include "WebsocketGameInstance.h"

void UWebsocketGameInstance::Init()
{
	Super::Init();
	h.set_open_listener(std::bind(&UWebsocketGameInstance::on_connected, this));
	h.set_fail_listener(std::bind(&UWebsocketGameInstance::on_fail, this));
	h.set_close_listener(std::bind(&UWebsocketGameInstance::on_disconnected, this));
}

void UWebsocketGameInstance::WS_Connect(FString url)
{
	h.connect(std::string(TCHAR_TO_UTF8(*url)));
}

void UWebsocketGameInstance::WS_Disconect()
{
	h.close();
	IsSioConnected = false;
}

void UWebsocketGameInstance::WS_Emit(FString name, FString msglist)
{
	h.socket()->emit(TCHAR_TO_UTF8(*name), std::string(TCHAR_TO_UTF8(*msglist)));
}

bool UWebsocketGameInstance::WS_IsConnected()
{
	return IsSioConnected;
}

void UWebsocketGameInstance::on_connected()
{
	IsSioConnected = true;
	OnConnected();
}

void UWebsocketGameInstance::on_fail()
{
	IsSioConnected = false;
	OnFail();
}

void UWebsocketGameInstance::on_disconnected()
{
	IsSioConnected = true;
	OnDisconnected();
}
