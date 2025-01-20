/*
 * Copyright (c) 2023-2025 Nathan Miguel
 *
 * InternetProtocol is free library: you can redistribute it and/or modify it under the terms
 * of the GNU Affero General Public License as published by the Free Software Foundation,
 * version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
*/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UDP/UDPServer.h"
#include "UDP/UDPClient.h"
#include "TCP/TCPClient.h"
#include "Http/HttpClient.h"
#include "WS/WSClient.h"
#include "Json/JavaScriptObjectNotation.h"
#include "InternetProtocolSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class INTERNETPROTOCOL_API UInternetProtocolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UUDPServer* CreateUDPServer();
	
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
	UWSClient* CreateWebsocketClient();

	UFUNCTION(BlueprintCallable, Category="IP")
	UWSClientSsl* CreateWebsocketClientSsl();

	UFUNCTION(BlueprintCallable, Category="IP")
	UJavaScriptObjectNotation* CreateJsonObject();
	
};
