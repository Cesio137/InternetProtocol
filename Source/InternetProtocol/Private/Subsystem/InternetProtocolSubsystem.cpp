/*
 * Copyright (c) 2023-2024 Nathan Miguel
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
