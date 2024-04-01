// Fill out your copyright notice in the Description page of Project Settings.


#include "JavaScriptObjectNotation.h"

UJavaScriptObjectNotation::UJavaScriptObjectNotation()
{
	
}

UJavaScriptObjectNotation::~UJavaScriptObjectNotation()
{
	Json.Reset();
}

bool UJavaScriptObjectNotation::ConstructJsonFromObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (Json.IsValid()) Reset();
	if (!JsonObject.IsValid()) return false;
	Json = JsonObject;
	return true;
}

TSharedPtr<FJsonObject> UJavaScriptObjectNotation::GetJsonObject()
{
	return Json;
}

bool UJavaScriptObjectNotation::ConstructJson()
{
	if (Json.IsValid()) Reset();
	Json = MakeShareable(new FJsonObject());
	return Json.IsValid();
}

bool UJavaScriptObjectNotation::ConstructJsonFromString(const FString& Data)
{
	if (Json.IsValid()) Reset();
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Data);
	bool success = FJsonSerializer::Deserialize(JsonReader, Json) && Json.IsValid();
	return success;
}

FString UJavaScriptObjectNotation::ToString()
{
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);

	return JsonString;
}

void UJavaScriptObjectNotation::Reset()
{
	Json.Reset();
}

bool UJavaScriptObjectNotation::HasField(const FString& FieldName)
{
	if (!Json.IsValid()) return false;
	return Json->HasField(FieldName);
}

void UJavaScriptObjectNotation::RemoveField(const FString& FieldName)
{
	if (!Json.IsValid()) return;
	Json->RemoveField(FieldName);
}

void UJavaScriptObjectNotation::SetBoolField(const FString& FieldName, const bool& Value)
{
	if (!Json.IsValid()) return;
	Json->SetBoolField(FieldName, Value);
}

void UJavaScriptObjectNotation::SetByteField(const FString& FieldName, const uint8& Value)
{
	if (!Json.IsValid()) return;
	double d = static_cast<double>(Value);
	Json->SetNumberField(FieldName, d);
}

void UJavaScriptObjectNotation::SetIntegerField(const FString& FieldName, const int64& Value)
{
	if (!Json.IsValid()) return;
	double d = static_cast<double>(Value);
	Json->SetNumberField(FieldName, d);
}

void UJavaScriptObjectNotation::SetFloatField(const FString& FieldName, const float& Value)
{
	if (!Json.IsValid()) return;
	double d = static_cast<double>(Value);
	Json->SetNumberField(FieldName, d);
}

void UJavaScriptObjectNotation::SetStringField(const FString& FieldName, const FString& Value)
{
	if (!Json.IsValid()) return;
	Json->SetStringField(FieldName, Value);
}

void UJavaScriptObjectNotation::SetObjectField(const FString& FieldName, UJavaScriptObjectNotation* Value)
{
	if (!Json.IsValid()) return;
	if (Value != nullptr)
		Json->SetObjectField(FieldName, Value->GetJsonObject());		
}

void UJavaScriptObjectNotation::SetBoolArrayField(const FString& FieldName, const TArray<bool>& Value)
{
	if (!Json.IsValid()) return;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (bool Bool : Value)
	{
		JsonArray.Add(MakeShareable(new FJsonValueNumber(Bool)));
	}
	Json->SetArrayField(FieldName, JsonArray);
}

void UJavaScriptObjectNotation::SetByteArrayField(const FString& FieldName, const TArray<uint8>& Value)
{
	if (!Json.IsValid()) return;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (uint8 Byte : Value)
	{
		JsonArray.Add(MakeShareable(new FJsonValueNumber(Byte)));
	}
	Json->SetArrayField(FieldName, JsonArray);
}

void UJavaScriptObjectNotation::SetIntegerArrayField(const FString& FieldName, const TArray<int64>& Value)
{
	if (!Json.IsValid()) return;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (int64 Integer : Value)
	{
		JsonArray.Add(MakeShareable(new FJsonValueNumber(Integer)));
	}
	Json->SetArrayField(FieldName, JsonArray);
}

void UJavaScriptObjectNotation::SetFloatArrayField(const FString& FieldName, const TArray<int64>& Value)
{
	if (!Json.IsValid()) return;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (float f : Value)
	{
		JsonArray.Add(MakeShareable(new FJsonValueNumber(f)));
	}
	Json->SetArrayField(FieldName, JsonArray);
}

void UJavaScriptObjectNotation::SetStringArrayField(const FString& FieldName, const TArray<FString>& Value)
{
	if (!Json.IsValid()) return;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (FString String : Value)
	{
		TSharedPtr<FJsonValue> JsonValue = MakeShareable(new FJsonValueString(String));
		JsonArray.Add(JsonValue);
	}
	Json->SetArrayField(FieldName, JsonArray);
}

void UJavaScriptObjectNotation::SetObjectArrayField(const FString& FieldName, const TArray<UJavaScriptObjectNotation*>& Value)
{
	if (!Json.IsValid()) return;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (UJavaScriptObjectNotation* JsonObject : Value)
	{
		if (JsonObject != nullptr)
		{
			TSharedPtr<FJsonValue> JsonValue = MakeShareable(new FJsonValueObject(JsonObject->GetJsonObject()));
			JsonArray.Add(JsonValue);
		}
	}
	Json->SetArrayField(FieldName, JsonArray);
}

bool UJavaScriptObjectNotation::TryGetBoolField(const FString& FieldName)
{
	if (!Json.IsValid()) return false;
	if (!HasField(FieldName)) return false;
	return Json->GetBoolField(FieldName);
}

bool UJavaScriptObjectNotation::TryGetByteField(const FString& FieldName, uint8& Value)
{
	if (!Json.IsValid()) return false;
	int i;
	bool success = Json->TryGetNumberField(FieldName, i);
	if (!success) return success;
	Value = static_cast<uint8>(i);
	return success;
}

bool UJavaScriptObjectNotation::TryGetIntegerField(const FString& FieldName, int64& Value)
{
	if (!Json.IsValid()) return false;
	return Json->TryGetNumberField(FieldName, Value);
}

bool UJavaScriptObjectNotation::TryGetFloatField(const FString& FieldName, float& Value)
{
	if (!Json.IsValid()) return false;
	double val;
	bool success = Json->TryGetNumberField(FieldName, val);
	if (!success) return false;
	Value = static_cast<float>(val);
	return success;
}

bool UJavaScriptObjectNotation::TryGetStringField(const FString& FieldName, FString& Value)
{
	if (!Json.IsValid()) return false;
	return Json->TryGetStringField(FieldName, Value);
}

bool UJavaScriptObjectNotation::TryGetObjectField(const FString& FieldName,  UJavaScriptObjectNotation*& Value)
{
	if (!Json.IsValid()) return false;
	bool success = false;
	const TSharedPtr<FJsonObject>* JsonObject;
	success = Json->TryGetObjectField(FieldName, JsonObject);
	if (!success)
	{
		delete JsonObject;
		return success;
	}
	UJavaScriptObjectNotation* NewJson = NewObject<UJavaScriptObjectNotation>();
	if (!NewJson)
	{
		delete JsonObject;
		delete NewJson;
		return false;
	}
	success = NewJson->ConstructJsonFromObject(*JsonObject);
	if (!success)
	{
		delete JsonObject;
		delete NewJson;
		return success;
	}
	Value = NewJson;
	return success;
}

bool UJavaScriptObjectNotation::TryGetBoolArrayField(const FString& FieldName, TArray<bool>& Value)
{
	if (Json.IsValid()) return false;
	if (!HasField(FieldName)) return false;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	JsonArray = Json->GetArrayField(FieldName);
	TArray<bool> data;
	for (TSharedPtr<FJsonValue> Val : JsonArray)
	{
		if (Val.IsValid() && Val->Type == EJson::Boolean)
		{
			data.Add(Val->AsBool());
		}
	}
	return false;
}

bool UJavaScriptObjectNotation::TryGetByteArrayField(const FString& FieldName, TArray<uint8>& Value)
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

bool UJavaScriptObjectNotation::TryGetIntegerArrayField(const FString& FieldName, TArray<int64>& Value)
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

bool UJavaScriptObjectNotation::TryGetFloatArrayField(const FString& FieldName, TArray<float>& Value)
{
	if (Json.IsValid()) return false;
	if (!HasField(FieldName)) return false;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	JsonArray = Json->GetArrayField(FieldName);
	TArray<float> data;
	for (TSharedPtr<FJsonValue> Val : JsonArray)
	{
		if (Val.IsValid() && Val->Type == EJson::Number)
		{
			data.Add(static_cast<float>(Val->AsNumber()));
		}
	}
	return true;
}

bool UJavaScriptObjectNotation::TryGetStringArrayField(const FString& FieldName, TArray<FString>& Value)
{
	if (Json.IsValid()) return false;
	return Json->TryGetStringArrayField(FieldName, Value);
}

bool UJavaScriptObjectNotation::TryGetObjectArrayField(const FString& FieldName, TArray<UJavaScriptObjectNotation*>& Value)
{
	if (Json.IsValid()) return false;
	if (!HasField(FieldName)) return false;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	JsonArray = Json->GetArrayField(FieldName);
	TArray<UJavaScriptObjectNotation*> data;
	for (TSharedPtr<FJsonValue> Val : JsonArray)
	{
		if (Val.IsValid() && Val->Type == EJson::Object)
		{
			data.Add(NewObject<UJavaScriptObjectNotation>());
			data.Last()->ConstructJsonFromObject(Val->AsObject());
		}
	}
	return false;
}
