// Fill out your copyright notice in the Description page of Project Settings.


#include "Websocket/Websocket.h"

void UWebsocket::BeginDestroy()
{
	UObject::BeginDestroy();

	if (websocket.IsValid())
	{
		websocket->OnConnected().Clear();
		websocket->OnConnectionError().Clear();
		websocket->OnClosed().Clear();
		websocket->OnMessage().Clear();
		websocket->OnMessageSent().Clear();
		websocket->OnRawMessage().Clear();
	}
	websocket.Reset();
}

void UWebsocket::ConstructWebsocket(TEnumAsByte<EOutputExecPins>& Output, const FString& url, const FString& protocol)
{
	websocket = FWebSocketsModule::Get().CreateWebSocket(url, protocol);
	if (!websocket.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}

	websocket->OnConnected().AddLambda([&]() -> void
	{
		OnConnected.Broadcast();
	});
	websocket->OnConnectionError().AddLambda([&](const FString& Error) -> void
	{
		OnConnectionError.Broadcast(Error);
	});
	websocket->OnClosed().AddLambda([&](int32 StatusCode, const FString& Reason, bool bWasClean) -> void
	{
		OnClosed.Broadcast(StatusCode, Reason, bWasClean);
	});
	websocket->OnMessage().AddLambda([&](const FString& Message) -> void
	{
		OnMessage.Broadcast(Message);
	});
	websocket->OnMessageSent().AddLambda([&](const FString& MessageString) -> void
	{
		OnMessageSent.Broadcast(MessageString);
	});
	websocket->OnRawMessage().AddLambda([&](const void* Data, SIZE_T Size, SIZE_T BytesRemaining) -> void
	{
		FVoid data;
		data.Data = Data;

		OnRawMessage.Broadcast(data, Size, BytesRemaining);
	});

	Output = EOutputExecPins::Success;
}

void UWebsocket::Connect()
{
	if (!websocket.IsValid()) return;
	if (!websocket->IsConnected())
		websocket->Connect();
}

void UWebsocket::Close(int32 code, const FString& reason)
{
	if (!websocket.IsValid()) return;
	websocket->Close(code, reason);
}

bool UWebsocket::bIsConnected()
{
	if (!websocket.IsValid()) return false;
	return websocket->IsConnected();
}

bool UWebsocket::bIsWebsocketValid()
{
	return websocket.IsValid();
}

void UWebsocket::Reset()
{
	if (websocket.IsValid())
	{
		websocket->OnConnected().Clear();
		websocket->OnConnectionError().Clear();
		websocket->OnClosed().Clear();
		websocket->OnMessage().Clear();
		websocket->OnMessageSent().Clear();
		websocket->OnRawMessage().Clear();
	}
	websocket.Reset();
}

void UWebsocket::Send(const FString& Data)
{
	if (!websocket.IsValid()) return;
	websocket->Send(Data);
}

void UWebsocket::SendRaw(const TArray<uint8>& Data)
{
	if (!websocket.IsValid()) return;
	websocket->Send(Data.GetData(), Data.Num(), true);
}
