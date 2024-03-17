// Fill out your copyright notice in the Description page of Project Settings.


#include "WebsocketGameInstance.h"
#include <iostream>

void UWebsocketGameInstance::Init()
{
	Super::Init();
	h.set_open_listener(std::bind(&UWebsocketGameInstance::on_connected, this));
	h.set_fail_listener(std::bind(&UWebsocketGameInstance::on_fail, this));
	h.set_close_listener(std::bind(&UWebsocketGameInstance::on_disconnected, this));
}

void UWebsocketGameInstance::BeginDestroy()
{
	Super::BeginDestroy();
	if (bIsConnected)
		h.close();
}


void UWebsocketGameInstance::WS_Connect(const FString &url)
{
	h.connect(std::string(TCHAR_TO_UTF8(*url)));
}

void UWebsocketGameInstance::WS_Disconect()
{
	h.close();
	bIsConnected = false;
}

void UWebsocketGameInstance::WS_EmitStrMessage(const FString& EventName, FString message)
{
	h.socket()->emit(TCHAR_TO_UTF8(*EventName), std::string(TCHAR_TO_UTF8(*message)));
}

void UWebsocketGameInstance::WS_EmitRawMessage(const FString& EventName, const TArray<uint8>& BinaryMessage)
{
	std::shared_ptr<std::string> byteArray_char = std::make_shared<std::string>(reinterpret_cast<const char*>( BinaryMessage.GetData() ),  BinaryMessage.Num() );
	h.socket()->emit(TCHAR_TO_UTF8(*EventName),byteArray_char);
}

bool UWebsocketGameInstance::WS_IsConnected()
{
	return bIsConnected;
}

FString UWebsocketGameInstance::WS_GetID()
{
	return h.get_sessionid().c_str();
}

void UWebsocketGameInstance::BindSocketEventStrByName(const FString& EventName, FDelegateSocketStrEvent WebsocketEvent)
{
	SocketStrEvent = WebsocketEvent;
	h.socket()->on(TCHAR_TO_UTF8(*EventName), sio::socket::event_listener([&](sio::event& Event)
	{
		const sio::message::ptr& message_received = Event.get_message();
		if (message_received->get_flag() == sio::message::flag_string)
		{
			std::string message = Event.get_message()->get_string();
			SocketStrEvent.ExecuteIfBound(message.c_str());
		}
	}));
}

void UWebsocketGameInstance::BindSocketEventRawByName(const FString& EventName, FDelegateSocketBinaryEvent WebsocketEvent)
{
	SocketBinaryEvent = WebsocketEvent;
	h.socket()->on(TCHAR_TO_UTF8(*EventName), sio::socket::event_listener([&](sio::event& Event)
	{
		const sio::message::ptr& message_received = Event.get_message();
		if (message_received->get_flag() == sio::message::flag_binary)
		{
			const std::shared_ptr<const std::string>& RawData = Event.get_message()->get_binary();
			const char* rawData = RawData->data();
			size_t size = RawData->size();

			TArray<uint8> BinaryData;
			BinaryData.Append(reinterpret_cast<const uint8*>(rawData), size);
			
			SocketBinaryEvent.ExecuteIfBound(BinaryData);
		}
	}));
}

void UWebsocketGameInstance::on_connected()
{
	bIsConnected = true;
	OnConnected();
}

void UWebsocketGameInstance::on_fail()
{
	bIsConnected = false;
	OnFail();
}

void UWebsocketGameInstance::on_disconnected()
{
	h.clear_socket_listeners();
	bIsConnected = false;
	OnDisconnected();
}
