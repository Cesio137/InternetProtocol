// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/Net/Message.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "WebsocketClient.generated.h"

/**
 * 
 */

//DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateOnConnected);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateOnMessage, const FString&, Message);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateOnRawMessage, const FVoid&, Data, int, Size, int, BytesRemaining);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateOnConnectionError, const FString&, Error);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateOnClosed, int32, StatusCode, const FString&, Reason, bool, bWasClean);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateOnMessageSent, const FString&, MessageString);

UCLASS()
class INTERNETPROTOCOL_API UWebsocketClient : public UObject
{
	GENERATED_BODY()
	
};
