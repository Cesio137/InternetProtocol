// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "Library/StructLibrary.h"
#include "Delegates.generated.h"

/**
 * 
 */

//HTTP
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateResponse, FRequest, Request, FResponse, Response, bool, Success);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateRetry, FRequest, Request, FResponse, Response, float, TimeToRetry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateError, int, Code, const FString&, exception);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateProgress, FRequest, Request, int, BytesSent, int, BytesReceived);

//Websocket
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateOnConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateOnMessage, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateOnRawMessage, const FVoid&, Data, int, Size, int, BytesRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateOnConnectionError, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateOnClosed, int32, StatusCode, const FString&, Reason, bool, bWasClean);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateOnMessageSent, const FString&, MessageString);

UCLASS(Blueprintable)
class INTERNETPROTOCOL_API UDelegates : public UObject
{
	GENERATED_BODY()
};
