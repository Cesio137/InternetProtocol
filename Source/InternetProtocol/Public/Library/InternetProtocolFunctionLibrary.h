// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Net/Message.h"
#include "InternetProtocolFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, Category = "IP")
class INTERNETPROTOCOL_API UInternetProtocolFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="IP||Buffer")
	static FString toString(const TArray<uint8>& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="IP||HTTP||Response")
	static TArray<FString> DeserializeHeaderLine(const FString& value);

	/*Binary functions*/

	// Convert bool to byte array | Convert byte array to bool	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static TArray<uint8> BoolToByteArray(bool value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static bool ByteArrayToBoolean(const TArray<uint8>& ByteArray);

	// Convert int to byte array | Convert byte array to int
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static TArray<uint8> IntToByteArray(const int& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static int ByteArrayToInt(const TArray<uint8>& ByteArray);

	// Convert float to byte array | Convert byte array to float
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static TArray<uint8> FloatToByteArray(const float& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static float ByteArrayToFloat(const TArray<uint8>& ByteArray);

	// Convert FVector to byte array | Convert byte array to float
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static TArray<uint8> FVectorToByteArray(const FVector& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static FVector ByteArrayToFVector(const TArray<uint8>& ByteArray);

	// Convert FRotator to byte array | Convert byte array to float
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static TArray<uint8> FRotatorToByteArray(const FRotator& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static FRotator ByteArrayToFRotator(const TArray<uint8>& ByteArray);

	// Convert FRotator to byte array | Convert byte array to float
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static TArray<uint8> FTransformToByteArray(const FTransform& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static FTransform ByteArrayToFTransform(const TArray<uint8>& ByteArray);
};