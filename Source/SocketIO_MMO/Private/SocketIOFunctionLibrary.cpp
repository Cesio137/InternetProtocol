// Fill out your copyright notice in the Description page of Project Settings.


#include "SocketIOFunctionLibrary.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

TArray<uint8> USocketIOFunctionLibrary::BoolToByteArray(const bool& value)
{
	bool boolean = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << boolean;
	return ByteArray;
}

bool USocketIOFunctionLibrary::ByteArrayToBoolean(const TArray<uint8>& ByteArray)
{
	bool boolean;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << boolean;
	return boolean;
}

TArray<uint8> USocketIOFunctionLibrary::IntToByteArray(const int& value)
{
	int i = value;
	TArray<uint8> ByteArray;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << ByteArray;
	return ByteArray;
}

int USocketIOFunctionLibrary::ByteArrayToInt(const TArray<uint8>& ByteArray)
{
	int i;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << i;
	return i;
}

TArray<uint8> USocketIOFunctionLibrary::FloatToByteArray(const float& value)
{
	float f = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << f;
	return ByteArray;
}

float USocketIOFunctionLibrary::ByteArrayToFloat(const TArray<uint8>& ByteArray)
{
	float f;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << f;
	return f;
}

TArray<uint8> USocketIOFunctionLibrary::FVectorToByteArray(const FVector& value)
{
	FVector Vector = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << Vector;
	return ByteArray;
}

FVector USocketIOFunctionLibrary::ByteArrayToFVector(const TArray<uint8>& ByteArray)
{
	FVector Vector;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Vector;
	return Vector;
}

TArray<uint8> USocketIOFunctionLibrary::FRotatorToByteArray(const FRotator& value)
{
	FRotator Rotator = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << Rotator;
	return ByteArray;
}

FRotator USocketIOFunctionLibrary::ByteArrayToFRotator(const TArray<uint8>& ByteArray)
{
	FRotator Rotator;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Rotator;
	return Rotator;
}
