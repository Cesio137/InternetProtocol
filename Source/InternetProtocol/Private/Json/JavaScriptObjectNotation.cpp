/*
 * Copyright (c) 2023-2024 Nathan Miguel
 *
 * InternetProtocol is free library: you can redistribute it and/or modify it under the terms
 * of the GNU Affero General Public License as published by the Free Software Foundation,
 * version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
*/


#include "Json/JavaScriptObjectNotation.h"

void UJavaScriptObjectNotation::BeginDestroy()
{
	UObject::BeginDestroy();
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

void UJavaScriptObjectNotation::ConstructJson(TEnumAsByte<EOutputExecPins>& Output)
{
	if (Json.IsValid()) Reset();
	Json = MakeShareable(new FJsonObject());
	if (Json.IsValid())
	{
		Output = EOutputExecPins::Success;
		return;
	}
	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::ConstructJsonFromString(TEnumAsByte<EOutputExecPins>& Output, const FString& Data)
{
	if (Json.IsValid()) Reset();
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Data);
	if (FJsonSerializer::Deserialize(JsonReader, Json) && Json.IsValid())
	{
		Output = EOutputExecPins::Success;
		return;
	}
	Output = EOutputExecPins::Failure;
}

FString UJavaScriptObjectNotation::ToString()
{
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);

	return JsonString;
}

bool UJavaScriptObjectNotation::bIsValid()
{
	return Json.IsValid();
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
	double number = static_cast<double>(Value);
	Json->SetNumberField(FieldName, number);
}

void UJavaScriptObjectNotation::SetIntegerField(const FString& FieldName, const int Value)
{
	if (!Json.IsValid()) return;
	double number = static_cast<double>(Value);
	Json->SetNumberField(FieldName, number);
}

void UJavaScriptObjectNotation::SetInteger64Field(const FString& FieldName, const int64 Value)
{
	if (!Json.IsValid()) return;
	double number = static_cast<double>(Value);
	Json->SetNumberField(FieldName, number);
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

void UJavaScriptObjectNotation::SetIntegerArrayField(const FString& FieldName, const TArray<int>& Value)
{
	if (!Json.IsValid()) return;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (int Integer : Value)
	{
		JsonArray.Add(MakeShareable(new FJsonValueNumber(Integer)));
	}
	Json->SetArrayField(FieldName, JsonArray);
}

void UJavaScriptObjectNotation::SetInteger64ArrayField(const FString& FieldName, const TArray<int64>& Value)
{
	if (!Json.IsValid()) return;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (int64 Integer : Value)
	{
		JsonArray.Add(MakeShareable(new FJsonValueNumber(Integer)));
	}
	Json->SetArrayField(FieldName, JsonArray);
}

void UJavaScriptObjectNotation::SetFloatArrayField(const FString& FieldName, const TArray<float>& Value)
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

void UJavaScriptObjectNotation::TryGetBoolField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, bool& Value)
{
	Value = false;
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	if (Json->TryGetBoolField(FieldName, Value))
	{
		Output = EOutputExecPins::Success;
		return;
	}
	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::TryGetByteField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, uint8& Value)
{
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	int number;
	if (Json->TryGetNumberField(FieldName, number))
	{
		Value = static_cast<uint8>(number);
		Output = EOutputExecPins::Success;
		return;
	}
	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::TryGetIntegerField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, int& Value)
{
	if (!Json.IsValid()) 
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	if (Json->TryGetNumberField(FieldName, Value))
	{
		Output = EOutputExecPins::Success;
		return;
	}
	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::TryGetInteger64Field(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, int64& Value)
{
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	if (Json->TryGetNumberField(FieldName, Value))
	{
		Output = EOutputExecPins::Success;
		return;
	}
	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::TryGetFloatField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, float& Value)
{
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	double val;
	if (Json->TryGetNumberField(FieldName, val))
	{
		Value = static_cast<float>(val);
		Output = EOutputExecPins::Success;
		return;
	}
	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::TryGetStringField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, FString& Value)
{
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	if (Json->TryGetStringField(FieldName, Value))
	{
		Output = EOutputExecPins::Success;
		return;
	}
	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::TryGetObjectField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, UJavaScriptObjectNotation*& Value)
{
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	const TSharedPtr<FJsonObject>* JsonObject;
	if (!Json->TryGetObjectField(FieldName, JsonObject))
	{
		delete JsonObject;
		Output = EOutputExecPins::Failure;
		return;
	}
	UJavaScriptObjectNotation* NewJson = NewObject<UJavaScriptObjectNotation>();
	if (!NewJson)
	{
		delete JsonObject;
		delete NewJson;
		Output = EOutputExecPins::Failure;
		return;
	}
	if (!NewJson->ConstructJsonFromObject(*JsonObject))
	{
		delete JsonObject;
		delete NewJson;
		Output = EOutputExecPins::Failure;
		return;
	}
	Value = NewJson;
	Output = EOutputExecPins::Success;
}

void UJavaScriptObjectNotation::TryGetBoolArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<bool>& Value)
{
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (Json->TryGetArrayField(FieldName, JsonArray))
	{
		TArray<bool> data;
		for (TSharedPtr<FJsonValue> Val : *JsonArray)
		{
			if (Val.IsValid() && Val->Type == EJson::Boolean)
			{
				data.Add(Val->AsBool());
			}
		}
		Value = data;
		Output = EOutputExecPins::Success;
		return;
	}
	
	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::TryGetByteArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<uint8>& Value)
{
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (Json->TryGetArrayField(FieldName, JsonArray))
	{
		TArray<uint8> data;
		for (TSharedPtr<FJsonValue> Val : *JsonArray)
		{
			if (Val.IsValid() && Val->Type == EJson::Number)
			{
				data.Add(static_cast<uint8>(Val->AsNumber()));
			}
		}
		Output = EOutputExecPins::Success;
		return;
	}
	
	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::TryGetIntegerArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<int>& Value)
{
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (Json->TryGetArrayField(FieldName, JsonArray))
	{
		TArray<int> data;
		for (TSharedPtr<FJsonValue> Val : *JsonArray)
		{
			if (Val.IsValid() && Val->Type == EJson::Number)
			{
				data.Add(static_cast<int>(Val->AsNumber()));
			}
		}
		Value = data;
		Output = EOutputExecPins::Success;
		return;
	}
	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::TryGetInteger64ArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<int64>& Value)
{
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (Json->TryGetArrayField(FieldName, JsonArray))
	{
		TArray<int64> data;
		for (TSharedPtr<FJsonValue> Val : *JsonArray)
		{
			if (Val.IsValid() && Val->Type == EJson::Number)
			{
				data.Add(static_cast<int64>(Val->AsNumber()));
			}
		}
		Value = data;
		Output = EOutputExecPins::Success;
		return;
	}
	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::TryGetFloatArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<float>& Value)
{
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (Json->TryGetArrayField(FieldName, JsonArray))
	{
		TArray<float> data;
		for (TSharedPtr<FJsonValue> Val : *JsonArray)
		{
			if (Val.IsValid() && Val->Type == EJson::Number)
			{
				data.Add(static_cast<float>(Val->AsNumber()));
			}
		}
		Value = data;
		Output = EOutputExecPins::Success;
		return;
	}

	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::TryGetStringArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<FString>& Value)
{
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	if (Json->TryGetStringArrayField(FieldName, Value))
	{
		Output = EOutputExecPins::Success;
		return;
	}
	Output = EOutputExecPins::Failure;
}

void UJavaScriptObjectNotation::TryGetObjectArrayField(TEnumAsByte<EOutputExecPins>& Output, const FString& FieldName, TArray<UJavaScriptObjectNotation*>& Value)
{
	if (!Json.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (Json->TryGetArrayField(FieldName, JsonArray))
	{
		TArray<UJavaScriptObjectNotation*> data;
		for (TSharedPtr<FJsonValue> Val : *JsonArray)
		{
			if (Val.IsValid() && Val->Type == EJson::Object)
			{
				data.Add(NewObject<UJavaScriptObjectNotation>());
				data.Last()->ConstructJsonFromObject(Val->AsObject());
			}
		}
		Value = data;
		Output = EOutputExecPins::Success;
		return;
	}

	Output = EOutputExecPins::Failure;
}
