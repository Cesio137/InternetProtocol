// Fill out your copyright notice in the Description page of Project Settings.


#include "WebsocketGameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include <iostream>

void UWebsocketGameInstance::Shutdown()
{
	Super::Shutdown();
}

void UWebsocketGameInstance::WS_CreateWebsocket(const FString& url, const FString& protocol)
{
	ws = FWebSocketsModule::Get().CreateWebSocket(url, protocol);
	if (!ws) return;

	// Bind Websocket events
	ws->OnConnected().AddLambda([&]() -> void {
			WS_OnConnected();
		});
	ws->OnConnectionError().AddLambda([&](const FString& Error) -> void {
			WS_OnConnectionError(Error);
		});
	ws->OnClosed().AddLambda([&](int32 StatusCode, const FString& Reason, bool bWasClean) -> void {
			WS_OnClosed(StatusCode, Reason, bWasClean);
		});
	ws->OnMessage().AddLambda([&](const FString& Message) -> void {
			WS_OnMessage(Message);
		});
	ws->OnMessageSent().AddLambda([&](const FString& MessageString) -> void {
			WS_OnMessageSent(MessageString);
		});
}

void UWebsocketGameInstance::WS_Connect()
{
	if (!ws) return;
	if (!ws->IsConnected())
		ws->Connect();
}

bool UWebsocketGameInstance::WS_IsConnected()
{
	if (!ws) return false;
	return ws->IsConnected();
}

void UWebsocketGameInstance::WS_Close(int32 code, const FString& reason)
{
	if (!ws) return;
	ws->Close(code, reason);
}

void UWebsocketGameInstance::BindSocketEventByName(const FDelegateWebsocketEvent& Event)
{
	if (!ws) return;
	ws->OnMessage().AddUFunction(this, Event.GetFunctionName());
}

void UWebsocketGameInstance::WS_EmitString(const FString& EventName, const FString& message)
{
	if (!ws) return;

	// Create Json object
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

	// Add data "event" and "message to json
	JsonObject->SetStringField(TEXT("event"), EventName);
	JsonObject->SetStringField(TEXT("message"), message);

	// Convert json to Str message
	FString messageString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&messageString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	
	ws->Send(messageString);
}

void UWebsocketGameInstance::WS_EmitRaw(const FString& EventName, const TArray<uint8>& data)
{
	if (!ws) return;

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (uint8 Byte : data)
	{
		// Converta cada uint8 para FJsonValue.
		JsonArray.Add(MakeShareable(new FJsonValueNumber(Byte)));
	}

	// Create Json object
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

	// Add data "event" and "message to json
	JsonObject->SetStringField(TEXT("event"), EventName);
	JsonObject->SetArrayField("data", JsonArray);

	// Convert json to Str message
	FString messageString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&messageString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	ws->Send(messageString);
}
