// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Library/EnumLibrary.h"
#include "StructLibrary.generated.h"
/**
 * 
 */

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
