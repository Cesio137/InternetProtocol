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

URequestObject* UInternetProtocolSubsystem::CreateRequestObject()
{
	URequestObject* request = NewObject<URequestObject>();
	return request;
}

UHttpClientObject* UInternetProtocolSubsystem::CreateHttpClientObject()
{
	UHttpClientObject* httpClient = NewObject<UHttpClientObject>();
	return httpClient;
}

UJavaScriptObjectNotation* UInternetProtocolSubsystem::CreateJsonObject()
{
	UJavaScriptObjectNotation* Json = NewObject<UJavaScriptObjectNotation>();
	return Json;
}
