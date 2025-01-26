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
#include "TCP/TCPServer.h"
#include "TCP/TCPClient.h"
#include "Http/HttpServer.h"
#include "Http/HttpClient.h"
#include "WS/WSServer.h"
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
	
	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UUDPClient* CreateUDPClient();

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UTCPServer* CreateTcpServer();
	
	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UTCPClient* CreateTcpClient();

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UTCPServerSsl* CreateTcpServerSsl();
	
	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UTCPClientSsl* CreateTcpClientSsl();

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UHttpServer* CreateHttpServer();
	
	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UHttpClient* CreateHttpClient();

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UHttpServerSsl* CreateHttpServerSsl();

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UHttpClientSsl* CreateHttpClientSsl();

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UWSServer* CreateWebsocketServer();
	
	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UWSClient* CreateWebsocketClient();

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UWSClientSsl* CreateWebsocketClientSsl();

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UJavaScriptObjectNotation* CreateJsonObject();
	
};
