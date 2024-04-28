//© Nathan Miguel, 2024. All Rights Reserved.


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
