// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StructLibrary.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FBinaryData
{
	GENERATED_BODY()

	FString event;
	TArray<uint8> data;

};
