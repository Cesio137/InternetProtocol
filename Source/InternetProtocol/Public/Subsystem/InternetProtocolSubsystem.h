//© Nathan Miguel, 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Websocket/Websocket.h"
#include "HTTP/HTTPObject.h"
#include "HTTP/RequestObject.h"
#include "HTTP/HttpClientObject.h"
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
	UFUNCTION(BlueprintCallable, Category="IP")
	UWebsocket* CreateWebsocketObject();

	UFUNCTION(BlueprintCallable, Category="IP")
	UHTTPObject* CreateHttpObject();

	UFUNCTION(BlueprintCallable, Category="IP")
	URequestObject* CreateRequestObject();

	UFUNCTION(BlueprintCallable, Category="IP")
	UHttpClientObject* CreateHttpClientObject();
	
	UFUNCTION(BlueprintCallable, Category="IP")
	UJavaScriptObjectNotation* CreateJsonObject();
};
