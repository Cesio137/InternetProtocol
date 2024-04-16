// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/InternetProtocolSubsystem.h"

UWebsocket* UInternetProtocolSubsystem::CreateWebsocketObject()
{
	UWebsocket* WS = NewObject<UWebsocket>();
	return WS;
}

UHTTPObject* UInternetProtocolSubsystem::CreateHttpObject()
{
	UHTTPObject* http = NewObject<UHTTPObject>();
	return http;
}

UJavaScriptObjectNotation* UInternetProtocolSubsystem::CreateJsonObject()
{
	UJavaScriptObjectNotation* Json = NewObject<UJavaScriptObjectNotation>();
	return Json;
}
