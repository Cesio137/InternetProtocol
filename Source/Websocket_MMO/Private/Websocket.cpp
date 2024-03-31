// Fill out your copyright notice in the Description page of Project Settings.


#include "Websocket.h"
#include "IWebSocket.h"

UWebsocket::UWebsocket()
{
	
}

UWebsocket::~UWebsocket()
{
	ws.Reset();
}

bool UWebsocket::CreateWebsocket(const FString& url, const FString& protocol)
{
	ws = FWebSocketsModule::Get().CreateWebSocket(url, protocol);
	if (!ws.IsValid()) return false;

	ws->OnConnected().AddLambda([&]() -> void
	{
		OnConnected.Broadcast();
	});
	ws->OnConnectionError().AddLambda([&](const FString& Error) -> void
	{
		OnConnectionError.Broadcast(Error);
	});
	ws->OnClosed().AddLambda([&](int32 StatusCode, const FString& Reason, bool bWasClean) -> void
	{
		OnClosed.Broadcast(StatusCode, Reason, bWasClean);
	});
	ws->OnMessage().AddLambda([&](const FString& Message) -> void
	{
		OnMessage.Broadcast(Message);
	});
	ws->OnMessageSent().AddLambda([&](const FString& MessageString) -> void
	{
		OnMessageSent.Broadcast(MessageString);
	});
	ws->OnRawMessage().AddLambda([&](const void* Data, SIZE_T Size, SIZE_T BytesRemaining) -> void
	{
		TArray<uint8> data;
		data.SetNumUninitialized(Size);
		FMemory::Memcpy(data.GetData(), Data, Size);

		OnRawMessage.Broadcast(data, Size, BytesRemaining);
	});

	return true;
}

void UWebsocket::Connect()
{
	if (!ws.IsValid()) return;
	if (!ws->IsConnected())
		ws->Connect();
}

void UWebsocket::Close(int32 code, const FString& reason)
{
	if (!ws.IsValid()) return;
	ws->Close(code, reason);
}

bool UWebsocket::bIsConnected()
{
	if (!ws.IsValid()) return false;
	return ws->IsConnected();
}

bool UWebsocket::bIsWebsocketValid()
{
	return ws.IsValid();
}

void UWebsocket::Reset()
{
	ws.Reset();
}

void UWebsocket::Send(const FString& Data)
{
	if (!ws.IsValid()) return;
	ws->Send(Data);
}

void UWebsocket::SendRaw(const TArray<uint8>& Data)
{
	if (!ws.IsValid()) return;
	ws->Send(Data.GetData(), Data.Num(), true);
}
