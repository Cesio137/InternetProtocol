// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/EnumLibrary.h"
#include "StructLibrary.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FJsonData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString EventName;

	UPROPERTY(BlueprintReadWrite)
	bool bIsBinary;

	UPROPERTY(BlueprintReadWrite)
	FString Data;

	UPROPERTY(BlueprintReadWrite)
	TArray<uint8> RawData;

};
