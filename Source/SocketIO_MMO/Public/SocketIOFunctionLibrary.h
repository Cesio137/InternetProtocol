// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SocketIOFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class SOCKETIO_MMO_API USocketIOFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// Convert bool to byte array | Convert byte array to bool
	UFUNCTION(BlueprintCallable, Category="SocketIO || Binary")
	static TArray<uint8> BoolToByteArray(const bool& value);

	UFUNCTION(BlueprintCallable, Category="SocketIO || Binary")
	static bool ByteArrayToBoolean(const TArray<uint8>& ByteArray);
	
	// Convert int to byte array | Convert byte array to int
	UFUNCTION(BlueprintCallable, Category="SocketIO || Binary")
	static TArray<uint8> IntToByteArray(const int& value);

	UFUNCTION(BlueprintCallable, Category="SocketIO || Binary")
	static int ByteArrayToInt(const TArray<uint8>& ByteArray);
	
	// Convert float to byte array | Convert byte array to float
	UFUNCTION(BlueprintCallable, Category="SocketIO || Binary")
	static TArray<uint8> FloatToByteArray(const float& value);

	UFUNCTION(BlueprintCallable, Category="SocketIO || Binary")
	static float ByteArrayToFloat(const TArray<uint8>& ByteArray);

	// Convert float to byte array | Convert byte array to float
	UFUNCTION(BlueprintCallable, Category="SocketIO || Binary")
	static TArray<uint8> FVectorToByteArray(const FVector& value);

	UFUNCTION(BlueprintCallable, Category="SocketIO || Binary")
	static FVector ByteArrayToFVector(const TArray<uint8>& ByteArray);

	// Convert float to byte array | Convert byte array to float
	UFUNCTION(BlueprintCallable, Category="SocketIO || Binary")
	static TArray<uint8> FRotatorToByteArray(const FRotator& value);

	UFUNCTION(BlueprintCallable, Category="SocketIO || Binary")
	static FRotator ByteArrayToFRotator(const TArray<uint8>& ByteArray);
};
