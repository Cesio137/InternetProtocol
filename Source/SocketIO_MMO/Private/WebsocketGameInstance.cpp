// Fill out your copyright notice in the Description page of Project Settings.


#include "WebsocketGameInstance.h"
#include <iostream>

void UWebsocketGameInstance::Init()
{
	Super::Init();
	h.set_open_listener(std::bind(&UWebsocketGameInstance::on_connected, this));
	h.set_fail_listener(std::bind(&UWebsocketGameInstance::on_fail, this));
	h.set_close_listener(std::bind(&UWebsocketGameInstance::on_disconnected, this));
/*
	h.socket()->on("chatMessage", sio::socket::event_listener([&](sio::event& Event)
	{
		std::string message = Event.get_message()->get_string();
		chatMessage(message.c_str());
	}));

	h.socket()->on("playerName", sio::socket::event_listener([&](sio::event& Event)
	{
		std::string message = Event.get_message()->get_string();
		playerName(message.c_str());
	}));
*/
}

void UWebsocketGameInstance::BeginDestroy()
{
	Super::BeginDestroy();
	if (bIsConnected)
		h.close();
}


void UWebsocketGameInstance::WS_Connect(FString url)
{
	h.connect(std::string(TCHAR_TO_UTF8(*url)));
}

void UWebsocketGameInstance::WS_Disconect()
{
	h.close();
	bIsConnected = false;
}

void UWebsocketGameInstance::WS_Emit(FString name, FString msglist)
{
	h.socket()->emit(TCHAR_TO_UTF8(*name), std::string(TCHAR_TO_UTF8(*msglist)));
}

bool UWebsocketGameInstance::WS_IsConnected()
{
	return bIsConnected;
}

void UWebsocketGameInstance::BindSocketEventStrByName(FString EventName, FDelegateSocketStrEvent WebsocketEvent)
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

void UWebsocketGameInstance::BindSocketEventBoolByName(FString EventName, FDelegateSocketBoolEvent WebsocketEvent)
{
	SocketBoolEvent = WebsocketEvent;
	h.socket()->on(TCHAR_TO_UTF8(*EventName), sio::socket::event_listener([&](sio::event& Event)
	{
		const sio::message::ptr& message_received = Event.get_message();
		if (message_received->get_flag() == sio::message::flag_boolean)
		{
			SocketBoolEvent.ExecuteIfBound(Event.get_message()->get_bool());
		}
	}));
}

void UWebsocketGameInstance::BindSocketEventIntByName(FString EventName, FDelegateSocketIntEvent WebsocketEvent)
{
	SocketIntEvent = WebsocketEvent;
	h.socket()->on(TCHAR_TO_UTF8(*EventName), sio::socket::event_listener([&](sio::event& Event)
	{
		const sio::message::ptr& message_received = Event.get_message();
		if (message_received->get_flag() == sio::message::flag_integer)
		{
			SocketIntEvent.ExecuteIfBound(Event.get_message()->get_int());
		}
	}));
}

void UWebsocketGameInstance::BindSocketEventFloatByName(FString EventName, FDelegateSocketFloatEvent WebsocketEvent)
{
	SocketFloatEvent = WebsocketEvent;
	h.socket()->on(TCHAR_TO_UTF8(*EventName), sio::socket::event_listener([&](sio::event& Event)
	{
		const sio::message::ptr& message_received = Event.get_message();
		if (message_received->get_flag() == sio::message::flag_double)
		{
			SocketFloatEvent.ExecuteIfBound(static_cast<float>(Event.get_message()->get_double()));
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
