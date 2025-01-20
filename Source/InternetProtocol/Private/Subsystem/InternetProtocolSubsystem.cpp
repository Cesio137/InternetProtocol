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

UUDPServer* UInternetProtocolSubsystem::CreateUDPServer()
{
	UUDPServer* server = NewObject<UUDPServer>();
	return server;
}

UUDPClient* UInternetProtocolSubsystem::CreateUDPClient()
{
	UUDPClient* client = NewObject<UUDPClient>();
	return client;
}

UTCPClient* UInternetProtocolSubsystem::CreateTcpClient()
{
	UTCPClient* client = NewObject<UTCPClient>();
	return client;
}

UTCPClientSsl* UInternetProtocolSubsystem::CreateTcpClientSsl()
{
	UTCPClientSsl* client = NewObject<UTCPClientSsl>();
	return client;
}

UHttpClient* UInternetProtocolSubsystem::CreateHttpClient()
{
	UHttpClient* client = NewObject<UHttpClient>();
	return client;
}

UHttpClientSsl* UInternetProtocolSubsystem::CreateHttpClientSsl()
{
	UHttpClientSsl* client = NewObject<UHttpClientSsl>();
	return client;
}

UWSClient* UInternetProtocolSubsystem::CreateWebsocketClient()
{
	UWSClient* client = NewObject<UWSClient>();
	return client;
}

UWSClientSsl* UInternetProtocolSubsystem::CreateWebsocketClientSsl()
{
	UWSClientSsl* client = NewObject<UWSClientSsl>();
	return client;
}

UJavaScriptObjectNotation* UInternetProtocolSubsystem::CreateJsonObject()
{
	UJavaScriptObjectNotation* json = NewObject<UJavaScriptObjectNotation>();
	return json;
}
