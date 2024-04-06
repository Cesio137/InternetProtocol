// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Library/EnumLibrary.h"
#include "JavaScriptObjectNotation.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class INTERNETPROTOCOL_API UJavaScriptObjectNotation : public UObject
{
	GENERATED_BODY()
	UJavaScriptObjectNotation()
	{

	}
	~UJavaScriptObjectNotation()
	{
		Json.Reset();
	}

public:
	bool ConstructJsonFromObject(const TSharedPtr<FJsonObject>& JsonObject);
	TSharedPtr<FJsonObject> GetJsonObject();

public:
	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void ConstructJson(TEnumAsByte<EOutputExecPins>& Output);

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void ConstructJsonFromString(TEnumAsByte<EOutputExecPins>& Output, const FString& Data);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Json")
	FString ToString();

	UFUNCTION(BlueprintCallable, Category = "Json")
	void Reset();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Json")
	bool HasField(const FString& FieldName);

	UFUNCTION(BlueprintCallable, Category = "Json")
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

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void TryGetBoolField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, bool& Value);

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void TryGetByteField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, uint8& Value);

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void TryGetIntegerField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, int64& Value);

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void TryGetFloatField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, float& Value);

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void TryGetStringField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void TryGetObjectField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, UJavaScriptObjectNotation*& Value);

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void TryGetBoolArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<bool>& Value);

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void TryGetByteArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<uint8>& Value);

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void TryGetIntegerArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<int64>& Value);

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void TryGetFloatArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<float>& Value);

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void TryGetStringArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<FString>& Value);

	UFUNCTION(BlueprintCallable, Category = "Json", meta = (ExpandEnumAsExecs = "Output"))
	void TryGetObjectArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<UJavaScriptObjectNotation*>& Value);

private:
	TSharedPtr<FJsonObject> Json;
	
};
