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
	bool ConstructJsonFromObject(const TSharedPtr<FJsonObject>& JsonObject);
	TSharedPtr<FJsonObject> GetJsonObject();

public:	
	UFUNCTION(BlueprintCallable, Category="Json")
	bool ConstructJson();

	UFUNCTION(BlueprintCallable, Category="Json")
	bool ConstructJsonFromString(const FString& Data);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Json")
	FString ToString();

	UFUNCTION(BlueprintCallable, Category="Json")
	void Reset();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool HasField(const FString& FieldName);
	
	UFUNCTION(BlueprintCallable, Category="Json")
	void RemoveField(const FString& FieldName);

	UFUNCTION(BlueprintCallable, Category = "Json")
	void SetBoolField(const FString& FieldName, const bool& Value);

	UFUNCTION(BlueprintCallable, Category = "Json")
	void SetByteField(const FString& FieldName, const uint8& Value);

	UFUNCTION(BlueprintCallable, Category = "Json")
	void SetIntegerField(const FString& FieldName, const int64& Value);

	UFUNCTION(BlueprintCallable, Category = "Json")
	void SetFloatField(const FString& FieldName, const float& Value);

	UFUNCTION(BlueprintCallable, Category = "Json")
	void SetStringField(const FString& FieldName, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Json")
	void SetObjectField(const FString& FieldName, UJavaScriptObjectNotation* Value);

	UFUNCTION(BlueprintCallable, Category = "Json")
	void SetBoolArrayField(const FString& FieldName, const TArray<bool>& Value);

	UFUNCTION(BlueprintCallable, Category = "Json")
	void SetByteArrayField(const FString& FieldName, const TArray<uint8>& Value);

	UFUNCTION(BlueprintCallable, Category = "Json")
	void SetIntegerArrayField(const FString& FieldName, const TArray<int64>& Value);

	UFUNCTION(BlueprintCallable, Category = "Json")
	void SetFloatArrayField(const FString& FieldName, const TArray<int64>& Value);

	UFUNCTION(BlueprintCallable, Category = "Json")
	void SetStringArrayField(const FString& FieldName, const TArray<FString>& Value);

	UFUNCTION(BlueprintCallable, Category = "Json")
	void SetObjectArrayField(const FString& FieldName, const TArray<UJavaScriptObjectNotation*>& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetBoolField(const FString& FieldName);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Json")
	bool TryGetByteField(const FString& FieldName, uint8& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetIntegerField(const FString& FieldName, int64& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetFloatField(const FString& FieldName, float& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetStringField(const FString& FieldName, FString& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetObjectField(const FString& FieldName, UJavaScriptObjectNotation*& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Json")
	bool TryGetBoolArrayField(const FString& FieldName, TArray<bool>& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetByteArrayField(const FString& FieldName, TArray<uint8>& Value);	

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetIntegerArrayField(const FString& FieldName, TArray<int64>& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetFloatArrayField(const FString& FieldName, TArray<float>& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Json")
	bool TryGetStringArrayField(const FString& FieldName, TArray<FString>& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Json")
	bool TryGetObjectArrayField(const FString& FieldName, TArray<UJavaScriptObjectNotation*>& Value);
	
private:
	TSharedPtr<FJsonObject> Json;
};
