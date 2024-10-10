// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UDP/UDPClient.h"
#include "TCP/TCPClient.h"
#include "Http/HttpClient.h"
#include "Websockets/WebsocketClient.h"
#include "InternetProtocolSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class INTERNETPROTOCOL_API UInternetProtocolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="IP")
	UUDPClient* CreateUDPClient();

	UFUNCTION(BlueprintCallable, Category="IP")
	UTCPClient* CreateTcpClient();

	UFUNCTION(BlueprintCallable, Category="IP")
	UTCPClientSsl* CreateTcpClientSsl();

	UFUNCTION(BlueprintCallable, Category="IP")
	UHttpClient* CreateHttpClient();

	UFUNCTION(BlueprintCallable, Category="IP")
	UHttpClientSsl* CreateHttpClientSsl();

	UFUNCTION(BlueprintCallable, Category="IP")
	UWebsocketClient* CreateWebsocketClient();
	
};
