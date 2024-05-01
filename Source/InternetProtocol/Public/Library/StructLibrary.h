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
	UPROPERTY(BlueprintReadWrite, Category = "IP")
	ERequestStatus RequestStatus;
	
	UPROPERTY(BlueprintReadWrite, Category = "IP")
	float ElapsedTime;
};

USTRUCT(Blueprintable, Category = "IP")
struct FResponse
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "IP")
	int ResponseCode;

	UPROPERTY(BlueprintReadWrite, Category = "IP")
	TArray<uint8> Content;

	UPROPERTY(BlueprintReadWrite, Category = "IP")
	FString ContentAsString;
};