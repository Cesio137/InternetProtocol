// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JavaScriptObjectNotation.generated.h"

/**
 * 
 */

UCLASS(Blueprintable)
class WEBSOCKET_MMO_API UJavaScriptObjectNotation : public UObject
{
	GENERATED_BODY()
	UJavaScriptObjectNotation();
	~UJavaScriptObjectNotation();
public:
	UFUNCTION(BlueprintCallable, Category="Json")
	bool ConstructJson();

	UFUNCTION(BlueprintCallable, Category="Json")
	bool ConstructJsonFromString(const FString& Data);

	UFUNCTION(BlueprintCallable, Category="Json")
	void Reset();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool HasField(const FString& FieldName);
	
	UFUNCTION(BlueprintCallable, Category="Json")
	void RemoveField(const FString& FieldName);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetBoolField(const FString& FieldName);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetIntegerField(const FString& FieldName, int64& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetFloatField(const FString& FieldName, float& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetStringField(const FString& FieldName, FString& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetArrayByteField(const FString& FieldName, TArray<uint8>& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetArrayIntegerField(const FString& FieldName, TArray<int64>& Value);
	
private:
	TSharedPtr<FJsonObject> Json;
};
