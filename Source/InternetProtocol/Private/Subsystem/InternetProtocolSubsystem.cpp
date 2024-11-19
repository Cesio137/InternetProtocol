// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/InternetProtocolSubsystem.h"

UUDPClient* UInternetProtocolSubsystem::CreateUDPClient()
{
	UUDPClient* udpClient = NewObject<UUDPClient>();
	return udpClient;
}

UTCPClient* UInternetProtocolSubsystem::CreateTcpClient()
{
	UTCPClient* tcpClient = NewObject<UTCPClient>();
	return tcpClient;
}

UTCPClientSsl* UInternetProtocolSubsystem::CreateTcpClientSsl()
{
	UTCPClientSsl* tcpClient = NewObject<UTCPClientSsl>();
	return tcpClient;
}

UHttpClient* UInternetProtocolSubsystem::CreateHttpClient()
{
	UHttpClient* httpClient = NewObject<UHttpClient>();
	return httpClient;
}

UHttpClientSsl* UInternetProtocolSubsystem::CreateHttpClientSsl()
{
	UHttpClientSsl* httpClient = NewObject<UHttpClientSsl>();
	return httpClient;
}

UWebsocketClient* UInternetProtocolSubsystem::CreateWebsocketClient()
{
	UWebsocketClient* websocketClient = NewObject<UWebsocketClient>();
	return websocketClient;
}

UWebsocketClientSsl* UInternetProtocolSubsystem::CreateWebsocketClientSsl()
{
	UWebsocketClientSsl* websocketClient = NewObject<UWebsocketClientSsl>();
	return websocketClient;
}

UJavaScriptObjectNotation* UInternetProtocolSubsystem::CreateJsonObject()
{
	UJavaScriptObjectNotation* Json = NewObject<UJavaScriptObjectNotation>();
	return Json;
}