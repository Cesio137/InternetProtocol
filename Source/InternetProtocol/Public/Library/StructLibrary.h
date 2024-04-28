//© Nathan Miguel, 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Library/EnumLibrary.h"
#include "StructLibrary.generated.h"
/**
 * 
 */

//Websocket
USTRUCT(Blueprintable)
struct FVoid
{
	GENERATED_BODY()
	const void* Data;
};

//HTTP
USTRUCT(Blueprintable)
struct FRequest
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite)
	ERequestStatus RequestStatus;
	
	UPROPERTY(BlueprintReadWrite)
	float ElapsedTime;
};

USTRUCT(Blueprintable)
struct FResponse
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite)
	int ResponseCode;

	UPROPERTY(BlueprintReadWrite)
	TArray<uint8> Content;

	UPROPERTY(BlueprintReadWrite)
	FString ContentAsString;
};
