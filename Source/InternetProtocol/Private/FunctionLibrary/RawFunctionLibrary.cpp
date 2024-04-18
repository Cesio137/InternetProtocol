// Fill out your copyright notice in the Description page of Project Settings.


#include "FunctionLibrary/RawFunctionLibrary.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include <string>

bool URawFunctionLibrary::RawToBool(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data) == typeid(bool))
	{
		Output = EOutputExecPins::Success;
		return *(bool*)value.Data;
	}

	Output = EOutputExecPins::Failure;
	return false;
}

TArray<uint8> URawFunctionLibrary::RawToByteArray(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data) == typeid(TArray<uint8>))
	{
		Output = EOutputExecPins::Success;
		return *(TArray<uint8>*)value.Data;
	}

	Output = EOutputExecPins::Failure;
	return TArray<uint8>();
}

int64 URawFunctionLibrary::RawToInt(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data) == typeid(int))
	{
		Output = EOutputExecPins::Success;
		return *(int*)value.Data;
	}
	else if (typeid(value.Data) == typeid(int64))
	{
		Output = EOutputExecPins::Success;
		return *(int64*)value.Data;
	}

	Output = EOutputExecPins::Failure;
	return 0;
}

float URawFunctionLibrary::RawToFloat(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data) == typeid(float))
	{
		Output = EOutputExecPins::Success;
		return *(float*)value.Data;
	}

	Output = EOutputExecPins::Failure;
	return 0.0f;
}

FString URawFunctionLibrary::RawToString(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data) == typeid(FString))
	{
		Output = EOutputExecPins::Success;
		return *(FString*)value.Data;
	}
	else if (typeid(value.Data) == typeid(std::string))
	{
		std::string str = *(std::string*)value.Data;
		Output = EOutputExecPins::Success;
		return UTF8_TO_TCHAR(*str.c_str());
	}
	else if (typeid(value.Data) == typeid(const char*))
	{
		std::string str = *(std::string*)value.Data;
		Output = EOutputExecPins::Success;
		return UTF8_TO_TCHAR(*str.c_str());
	}

	Output = EOutputExecPins::Failure;
	return FString();
}

FVector URawFunctionLibrary::RawToFVector(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data) == typeid(FVector))
	{
		Output = EOutputExecPins::Success;
		return *(FVector*)value.Data;
	}

	Output = EOutputExecPins::Failure;
	return FVector();
}

FRotator URawFunctionLibrary::RawToFRotator(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data) == typeid(FRotator))
	{
		Output = EOutputExecPins::Success;
		return *(FRotator*)value.Data;
	}

	Output = EOutputExecPins::Failure;
	return FRotator();
}

FTransform URawFunctionLibrary::RawToFTransform(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data) == typeid(FTransform))
	{
		Output = EOutputExecPins::Success;
		return *(FTransform*)value.Data;
	}

	Output = EOutputExecPins::Failure;
	return FTransform();
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
