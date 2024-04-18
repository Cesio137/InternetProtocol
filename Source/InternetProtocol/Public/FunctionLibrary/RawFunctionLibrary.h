// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Library/StructLibrary.h"
#include "RawFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class INTERNETPROTOCOL_API URawFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

/*Raw functions*/

	// Convert bool to raw | Convert raw to bool
	UFUNCTION(BlueprintCallable, Category = "IP||Raw", meta = (ExpandEnumAsExecs = "Output"))
	static bool RawToBool(TEnumAsByte<EOutputExecPins>& Output, FVoid value);

	// Convert Byte Array to raw | Convert raw to Byte Array
	UFUNCTION(BlueprintCallable, Category = "IP||Raw", meta = (ExpandEnumAsExecs = "Output"))
	static TArray<uint8> RawToByteArray(TEnumAsByte<EOutputExecPins>& Output, FVoid value);

	// Convert int to raw | Convert raw to int
	UFUNCTION(BlueprintCallable, Category = "IP||Raw", meta = (ExpandEnumAsExecs = "Output"))
	static int64 RawToInt(TEnumAsByte<EOutputExecPins>& Output, FVoid value);

	// Convert int to float | Convert raw to float
	UFUNCTION(BlueprintCallable, Category = "IP||Raw", meta = (ExpandEnumAsExecs = "Output"))
	static float RawToFloat(TEnumAsByte<EOutputExecPins>& Output, FVoid value);

	// Convert int to float | Convert raw to float
	UFUNCTION(BlueprintCallable, Category = "IP||Raw", meta = (ExpandEnumAsExecs = "Output"))
	static FString RawToString(TEnumAsByte<EOutputExecPins>& Output, FVoid value);

	// Convert int to float | Convert raw to float
	UFUNCTION(BlueprintCallable, Category = "IP||Raw", meta = (ExpandEnumAsExecs = "Output"))
	static FVector RawToFVector(TEnumAsByte<EOutputExecPins>& Output, FVoid value);

	// Convert int to float | Convert raw to float
	UFUNCTION(BlueprintCallable, Category = "IP||Raw", meta = (ExpandEnumAsExecs = "Output"))
	static FRotator RawToFRotator(TEnumAsByte<EOutputExecPins>& Output, FVoid value);

	// Convert int to float | Convert raw to float
	UFUNCTION(BlueprintCallable, Category = "IP||Raw", meta = (ExpandEnumAsExecs = "Output"))
	static FTransform RawToFTransform(TEnumAsByte<EOutputExecPins>& Output, FVoid value);

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
