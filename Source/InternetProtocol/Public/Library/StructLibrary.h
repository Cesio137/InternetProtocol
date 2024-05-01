//© Nathan Miguel, 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Library/EnumLibrary.h"
#include "StructLibrary.generated.h"
/**
 * 
 */

//Websocket
USTRUCT(Blueprintable, Category = "IP")
struct FVoid
{
	GENERATED_BODY()
	const void* Data;
};

//HTTP
USTRUCT(Blueprintable, Category = "IP")
struct FRequest
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite)
	ERequestStatus RequestStatus;
	
	UPROPERTY(BlueprintReadWrite)
	float ElapsedTime;
};

USTRUCT(Blueprintable, Category = "IP")
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
