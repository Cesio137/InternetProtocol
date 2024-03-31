// Fill out your copyright notice in the Description page of Project Settings.


#include "WebsocketFunctionLibrary.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FJsonData UWebsocketFunctionLibrary::GetJsonData(const FString& JsonData)
{
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonData);
	TSharedPtr<FJsonObject> JsonObject;
	FJsonData data;

	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		data.EventName = JsonObject->GetStringField(TEXT("event"));
		data.bIsBinary = JsonObject->GetBoolField(TEXT("bIsBinary"));
		if (!data.bIsBinary) data.Data = JsonObject->GetStringField(TEXT("data"));
		if ( data.bIsBinary && JsonObject->TryGetField(TEXT("data")) )
		{
			TArray<TSharedPtr<FJsonValue>> JsonArray;
			JsonArray = JsonObject->GetArrayField(TEXT("data"));

			TArray<uint8> RawData;

			for (TSharedPtr<FJsonValue> Val : JsonArray)
			{
				if (Val.IsValid() && Val->Type == EJson::Number)
				{
					RawData.Add(static_cast<uint8>(Val->AsNumber()));
				}
			}
			data.RawData = RawData;
		}
	}
	return data;
}

TArray<uint8> UWebsocketFunctionLibrary::BoolToByteArray(const bool& value)
{
	bool boolean = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << boolean;
	return ByteArray;
}

bool UWebsocketFunctionLibrary::ByteArrayToBoolean(const TArray<uint8>& ByteArray)
{
	bool boolean;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << boolean;
	return boolean;
}

TArray<uint8> UWebsocketFunctionLibrary::IntToByteArray(const int& value)
{
	int i = value;
	TArray<uint8> ByteArray;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << ByteArray;
	return ByteArray;
}

int UWebsocketFunctionLibrary::ByteArrayToInt(const TArray<uint8>& ByteArray)
{
	int i;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << i;
	return i;
}

TArray<uint8> UWebsocketFunctionLibrary::FloatToByteArray(const float& value)
{
	float f = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << f;
	return ByteArray;
}

float UWebsocketFunctionLibrary::ByteArrayToFloat(const TArray<uint8>& ByteArray)
{
	float f;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << f;
	return f;
}

TArray<uint8> UWebsocketFunctionLibrary::FVectorToByteArray(const FVector& value)
{
	FVector Vector = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << Vector;
	return ByteArray;
}

FVector UWebsocketFunctionLibrary::ByteArrayToFVector(const TArray<uint8>& ByteArray)
{
	FVector Vector;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Vector;
	return Vector;
}

TArray<uint8> UWebsocketFunctionLibrary::FRotatorToByteArray(const FRotator& value)
{
	FRotator Rotator = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << Rotator;
	return ByteArray;
}

FRotator UWebsocketFunctionLibrary::ByteArrayToFRotator(const TArray<uint8>& ByteArray)
{
	FRotator Rotator;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Rotator;
	return Rotator;
}
