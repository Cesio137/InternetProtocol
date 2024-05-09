//© Nathan Miguel, 2024. All Rights Reserved.

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
	UFUNCTION(BlueprintCallable, Category = "IP||Raw")
	static bool RawToBool(FVoid value);

	// Convert Byte Array to raw | Convert raw to Byte Array
	
	UFUNCTION(BlueprintCallable, Category = "IP||Raw")
	static uint8 RawToByte(FVoid value);
	
	// Convert Byte Array to raw | Convert raw to Byte Array
	UFUNCTION(BlueprintCallable, Category = "IP||Raw")
	static TArray<uint8> RawToByteArray(FVoid value, int size);
	
	// Convert int to raw | Convert raw to int
	UFUNCTION(BlueprintCallable, Category = "IP||Raw")
	static int RawToInt(FVoid value);

	// Convert int64 to raw | Convert raw to int64
	UFUNCTION(BlueprintCallable, Category = "IP||Raw")
	static int64 RawToInt64(FVoid value);

	// Convert int to float | Convert raw to float
	UFUNCTION(BlueprintCallable, Category = "IP||Raw")
	static float RawToFloat(FVoid value);

	// Convert int to float | Convert raw to float
	UFUNCTION(BlueprintCallable, Category = "IP||Raw")
	static FString RawToString(FVoid value);

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
