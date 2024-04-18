// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Websocket/Websocket.h"
#include "HTTP/HTTPObject.h"
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
	UJavaScriptObjectNotation* CreateJsonObject();
};
