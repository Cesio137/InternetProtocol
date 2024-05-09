//© Nathan Miguel, 2024. All Rights Reserved.


#include "FunctionLibrary/RawFunctionLibrary.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include <string>
#include <typeinfo>


bool URawFunctionLibrary::RawToBool(FVoid value)
{
	int rawValue = *static_cast<const int*>(value.Data);
	return rawValue != 0;
}

uint8 URawFunctionLibrary::RawToByte(FVoid value)
{
	uint8 rawValue = *static_cast<const uint8*>(value.Data);
	return rawValue;
}

TArray<uint8> URawFunctionLibrary::RawToByteArray(FVoid value, int size)
{
	const uint8* rawByteArray = static_cast<const uint8*>(value.Data);
	TArray<uint8> rawValue;
	for (int i = 0; i < size; ++i) 
	{
		rawValue.Emplace(rawByteArray[i]);
	}
	return rawValue;
}

int URawFunctionLibrary::RawToInt(FVoid value)
{
	int rawValue = *static_cast<const int*>(value.Data);
	return rawValue;
}

int64 URawFunctionLibrary::RawToInt64(FVoid value)
{
	int64 rawValue = *static_cast<const int64*>(value.Data);
	return rawValue;
}

float URawFunctionLibrary::RawToFloat(FVoid value)
{
	int64 rawValue = *static_cast<const int64*>(value.Data);
	return rawValue;
}

FString URawFunctionLibrary::RawToString(FVoid value)
{
	const char* rawValue = static_cast<const char*>(value.Data);
	return rawValue;
}

TArray<uint8> URawFunctionLibrary::BoolToByteArray(bool value)
{
	bool boolean = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << boolean;
	return ByteArray;
}

bool URawFunctionLibrary::ByteArrayToBoolean(const TArray<uint8>& ByteArray)
{
	bool boolean;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << boolean;
	return boolean;
}

TArray<uint8> URawFunctionLibrary::IntToByteArray(const int& value)
{
	int integer = value;
	TArray<uint8> ByteArray;
	FMemoryWriter MemoryReader(ByteArray);
	MemoryReader << integer;
	return ByteArray;
}

int URawFunctionLibrary::ByteArrayToInt(const TArray<uint8>& ByteArray)
{
	int integer;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << integer;
	return integer;
}

TArray<uint8> URawFunctionLibrary::FloatToByteArray(const float& value)
{
	float f = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << f;
	return ByteArray;
}

float URawFunctionLibrary::ByteArrayToFloat(const TArray<uint8>& ByteArray)
{
	float f;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << f;
	return f;
}

TArray<uint8> URawFunctionLibrary::FVectorToByteArray(const FVector& value)
{
	FVector Vector = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << Vector;
	return ByteArray;
}

FVector URawFunctionLibrary::ByteArrayToFVector(const TArray<uint8>& ByteArray)
{
	FVector Vector;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Vector;
	return Vector;
}

TArray<uint8> URawFunctionLibrary::FRotatorToByteArray(const FRotator& value)
{
	FRotator Rotator = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << Rotator;
	return ByteArray;
}

FRotator URawFunctionLibrary::ByteArrayToFRotator(const TArray<uint8>& ByteArray)
{
	FRotator Rotator;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Rotator;
	return Rotator;
}

TArray<uint8> URawFunctionLibrary::FTransformToByteArray(const FTransform& value)
{
	FTransform Transform = value;
	TArray<uint8> ByteArray;
	FMemoryWriter MemoryReader(ByteArray);
	MemoryReader << Transform;
	return ByteArray;
}

FTransform URawFunctionLibrary::ByteArrayToFTransform(const TArray<uint8>& ByteArray)
{
	FTransform Transform;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Transform;
	return Transform;
}
