// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/InternetProtocolSubsystem.h"

UHttpClient* UInternetProtocolSubsystem::CreateHttpClient()
{
	UHttpClient* httpClient = NewObject<UHttpClient>();
	return httpClient;
}

UUDPClient* UInternetProtocolSubsystem::CreateUDPClient()
{
	UUDPClient* udpClient = NewObject<UUDPClient>();
	return udpClient;
}
