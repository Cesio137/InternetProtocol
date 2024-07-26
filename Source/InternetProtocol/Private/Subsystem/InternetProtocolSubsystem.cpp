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

UHttpClient* UInternetProtocolSubsystem::CreateHttpClient()
{
	UHttpClient* httpClient = NewObject<UHttpClient>();
	return httpClient;
}

UWebsocketClient* UInternetProtocolSubsystem::CreateWebsocketClient()
{
	UWebsocketClient* websocketClient = NewObject<UWebsocketClient>();
	return websocketClient;
}
