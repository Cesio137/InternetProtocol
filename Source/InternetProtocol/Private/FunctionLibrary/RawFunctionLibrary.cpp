//© Nathan Miguel, 2024. All Rights Reserved.


#include "FunctionLibrary/RawFunctionLibrary.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include <string>
#include <typeinfo>

FString URawFunctionLibrary::GetTypeName(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	return FString();
}

bool URawFunctionLibrary::RawToBool(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data).name() == typeid(bool).name())
	{
		Output = EOutputExecPins::Success;
		return *((bool*)value.Data);
	}

	Output = EOutputExecPins::Failure;
	return false;
}

uint8 URawFunctionLibrary::RawToByte(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data).name() == typeid(uint8).name())
	{
		Output = EOutputExecPins::Success;
		return *((uint8*)value.Data);
	}

	Output = EOutputExecPins::Failure;
	return 0;
}
/*
TArray<uint8> URawFunctionLibrary::RawToByteArray(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data) == typeid(uint8[]))
	{
		Output = EOutputExecPins::Success;
		return *(uint8*)value.Data;
	}

	Output = EOutputExecPins::Failure;
	return TArray<uint8>();
}
*/
int URawFunctionLibrary::RawToInt(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data).name() == typeid(int).name())
	{
		Output = EOutputExecPins::Success;
		return *((int*)value.Data);
	}

	Output = EOutputExecPins::Failure;
	return 0;
}

int64 URawFunctionLibrary::RawToInt64(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data).name() == typeid(int64).name())
	{
		Output = EOutputExecPins::Success;
		return *((int64*)value.Data);
	}

	Output = EOutputExecPins::Failure;
	return 0;
}

float URawFunctionLibrary::RawToFloat(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data).name() == typeid(float).name())
	{
		Output = EOutputExecPins::Success;
		return *((float*)value.Data);
	}

	Output = EOutputExecPins::Failure;
	return 0.0f;
}

FString URawFunctionLibrary::RawToString(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data).name() == typeid(FString).name())
	{
		Output = EOutputExecPins::Success;
		return *((FString*)value.Data);
	}
	else if (typeid(value.Data).name() == typeid(std::string).name())
	{
		std::string str = *((std::string*)value.Data);
		Output = EOutputExecPins::Success;
		return UTF8_TO_TCHAR(*str.c_str());
	}
	else if (typeid(value.Data).name() == typeid(const char*).name())
	{
		std::string str = *((std::string*)value.Data);
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
	if (typeid(value.Data).name() == typeid(FRotator).name())
	{
		Output = EOutputExecPins::Success;
		return *((FRotator*)value.Data);
	}

	Output = EOutputExecPins::Failure;
	return FRotator();
}

FTransform URawFunctionLibrary::RawToFTransform(TEnumAsByte<EOutputExecPins>& Output, FVoid value)
{
	if (typeid(value.Data).name() == typeid(FTransform).name())
	{
		Output = EOutputExecPins::Success;
		return *((FTransform*)value.Data);
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
