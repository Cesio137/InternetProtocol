// Fill out your copyright notice in the Description page of Project Settings.


#include "JavaScriptObjectNotation.h"

UJavaScriptObjectNotation::UJavaScriptObjectNotation()
{
	
}

UJavaScriptObjectNotation::~UJavaScriptObjectNotation()
{
	Json.Reset();
}

bool UJavaScriptObjectNotation::ConstructJson()
{
	if (Json.IsValid()) return false;
	Json = MakeShareable(new FJsonObject());
	return Json.IsValid();
}

bool UJavaScriptObjectNotation::ConstructJsonFromString(const FString& Data)
{
	if (Json.IsValid()) return false;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Data);
	bool success = FJsonSerializer::Deserialize(JsonReader, Json) && Json.IsValid();

	return success;
}

void UJavaScriptObjectNotation::Reset()
{
	Json.Reset();
}

bool UJavaScriptObjectNotation::HasField(const FString& FieldName)
{
	if (Json.IsValid()) return false;
	return Json->HasField(FieldName);
}

void UJavaScriptObjectNotation::RemoveField(const FString& FieldName)
{
	if (Json.IsValid()) return;
	Json->RemoveField(FieldName);
}

bool UJavaScriptObjectNotation::TryGetBoolField(const FString& FieldName)
{
	if (Json.IsValid()) return false;
	if (!HasField(FieldName)) return false;
	return Json->GetBoolField(FieldName);
}

bool UJavaScriptObjectNotation::TryGetIntegerField(const FString& FieldName, int64& Value)
{
	if (Json.IsValid()) return false;
	return Json->TryGetNumberField(FieldName, Value);
}

bool UJavaScriptObjectNotation::TryGetFloatField(const FString& FieldName, float& Value)
{
	if (Json.IsValid()) return false;
	double val;
	bool success = Json->TryGetNumberField(FieldName, val);
	Value = val;
	return  success;
}

bool UJavaScriptObjectNotation::TryGetStringField(const FString& FieldName, FString& Value)
{
	if (Json.IsValid()) return false;
	return Json->TryGetStringField(FieldName, Value);
}

bool UJavaScriptObjectNotation::TryGetArrayByteField(const FString& FieldName, TArray<uint8>& Value)
{
	if (Json.IsValid()) return false;
	if (!HasField(FieldName)) return false;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	JsonArray = Json->GetArrayField(FieldName);
	TArray<uint8> data;
	for (TSharedPtr<FJsonValue> Val : JsonArray)
	{
		if (Val.IsValid() && Val->Type == EJson::Number)
		{
			data.Add(static_cast<uint8>(Val->AsNumber()));
		}
	}
	return true;
}

bool UJavaScriptObjectNotation::TryGetArrayIntegerField(const FString& FieldName, TArray<int64>& Value)
{
	if (Json.IsValid()) return false;
	if (!HasField(FieldName)) return false;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	JsonArray = Json->GetArrayField(FieldName);
	TArray<int64> data;
	for (TSharedPtr<FJsonValue> Val : JsonArray)
	{
		if (Val.IsValid() && Val->Type == EJson::Number)
		{
			data.Add(static_cast<int64>(Val->AsNumber()));
		}
	}
	return true;
}
