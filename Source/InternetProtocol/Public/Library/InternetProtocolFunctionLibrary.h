// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/Net/Message.h"
#include "InternetProtocolFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class INTERNETPROTOCOL_API UInternetProtocolFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="IP||UDP||Message")
	static FString toString(const TArray<uint8>& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="IP||HTTP||Response")
	static TArray<FString> DeserializeHeaderLine(const FString& value);
};
