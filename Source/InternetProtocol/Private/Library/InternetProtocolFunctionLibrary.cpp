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

#include "Library/InternetProtocolFunctionLibrary.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include <string>
#include <typeinfo>

FString UInternetProtocolFunctionLibrary::toString(const TArray<uint8>& value)
{
	FUTF8ToTCHAR utf8_char(reinterpret_cast<const ANSICHAR*>(value.GetData()), value.Num());
	FString str(utf8_char.Length(), utf8_char.Get());
	return str;
}

TArray<FString> UInternetProtocolFunctionLibrary::DeserializeHeaderLine(const FString& value)
{
	TArray<FString> values;
	value.ParseIntoArray(values, TEXT(";"), true);
	for (FString& str : values)
	{
		FString Result = str;
		Result.TrimStartAndEndInline();
		str = Result;
	}

	return values;
}

TArray<uint8> UInternetProtocolFunctionLibrary::BoolToByteArray(bool value)
{
	bool boolean = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << boolean;
	return ByteArray;
}

bool UInternetProtocolFunctionLibrary::ByteArrayToBoolean(const TArray<uint8>& ByteArray)
{
	bool boolean;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << boolean;
	return boolean;
}

TArray<uint8> UInternetProtocolFunctionLibrary::IntToByteArray(const int& value)
{
	int integer = value;
	TArray<uint8> ByteArray;
	FMemoryWriter MemoryReader(ByteArray);
	MemoryReader << integer;
	return ByteArray;
}

int UInternetProtocolFunctionLibrary::ByteArrayToInt(const TArray<uint8>& ByteArray)
{
	int integer;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << integer;
	return integer;
}

TArray<uint8> UInternetProtocolFunctionLibrary::FloatToByteArray(const float& value)
{
	float f = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << f;
	return ByteArray;
}

float UInternetProtocolFunctionLibrary::ByteArrayToFloat(const TArray<uint8>& ByteArray)
{
	float f;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << f;
	return f;
}

TArray<uint8> UInternetProtocolFunctionLibrary::FVectorToByteArray(const FVector& value)
{
	FVector Vector = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << Vector;
	return ByteArray;
}

FVector UInternetProtocolFunctionLibrary::ByteArrayToFVector(const TArray<uint8>& ByteArray)
{
	FVector Vector;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Vector;
	return Vector;
}

TArray<uint8> UInternetProtocolFunctionLibrary::FRotatorToByteArray(const FRotator& value)
{
	FRotator Rotator = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << Rotator;
	return ByteArray;
}

FRotator UInternetProtocolFunctionLibrary::ByteArrayToFRotator(const TArray<uint8>& ByteArray)
{
	FRotator Rotator;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Rotator;
	return Rotator;
}

TArray<uint8> UInternetProtocolFunctionLibrary::FTransformToByteArray(const FTransform& value)
{
	FTransform Transform = value;
	TArray<uint8> ByteArray;
	FMemoryWriter MemoryReader(ByteArray);
	MemoryReader << Transform;
	return ByteArray;
}

FTransform UInternetProtocolFunctionLibrary::ByteArrayToFTransform(const TArray<uint8>& ByteArray)
{
	FTransform Transform;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Transform;
	return Transform;
}
