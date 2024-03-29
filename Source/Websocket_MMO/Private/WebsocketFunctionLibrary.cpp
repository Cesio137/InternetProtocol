// Fill out your copyright notice in the Description page of Project Settings.


#include "WebsocketFunctionLibrary.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool UWebsocketFunctionLibrary::GetMessageFromEvent(const FString& EventName, const FString& EventMessage, FString& Message)
{
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(EventMessage);

	TSharedPtr<FJsonObject> JsonObject;

	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		FString event;
		FString message;
		if (JsonObject->TryGetStringField(TEXT("event"), event))
		{
			if (event != EventName) return false;
			if (!JsonObject->TryGetStringField(TEXT("message"), message)) return false;
			Message = message;
			return true;
		}
	}
	return false;
}

bool UWebsocketFunctionLibrary::GetRawDataFromMessage(const FString& EventName, const FString& EventMessage, TArray<uint8>& Data)
{
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(EventMessage);

	TSharedPtr<FJsonObject> JsonObject;

	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		FString event;
		if (JsonObject->TryGetStringField(TEXT("event"), event))
		{
			TArray<TSharedPtr<FJsonValue>> JsonArray;

			if ( event != EventName ) return false;
			if (!JsonObject->TryGetField(TEXT("data"))) return false;
			JsonArray = JsonObject->GetArrayField(TEXT("data"));

			TArray<uint8> RawData;
			
			for (TSharedPtr<FJsonValue> Val : JsonArray)
			{
				if (Val.IsValid() && Val->Type == EJson::Number)
				{
					RawData.Add(static_cast<uint8>(Val->AsNumber()));
				}
			}
			
			Data = RawData;

			return true;
		}
	}
	
	return false;
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
