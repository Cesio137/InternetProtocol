//� Nathan Miguel, 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Library/EnumLibrary.h"
THIRD_PARTY_INCLUDES_START
#include "asio.hpp"
THIRD_PARTY_INCLUDES_END
#include "StructLibrary.generated.h"
/**
 * 
 */

//Websocket
USTRUCT(Blueprintable, Category = "IP")
struct FVoid
{
	GENERATED_BODY()
	const void* Data;
};

//HTTP
USTRUCT(Blueprintable, Category = "IP")
struct FRequest
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "IP")
	ERequestStatus RequestStatus;
	
	UPROPERTY(BlueprintReadWrite, Category = "IP")
	float ElapsedTime;
};

USTRUCT(Blueprintable, Category = "IP")
struct FResponse
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "IP")
	int ResponseCode;

	UPROPERTY(BlueprintReadWrite, Category = "IP")
	TArray<uint8> Content;

	UPROPERTY(BlueprintReadWrite, Category = "IP")
	FString ContentAsString;
};

struct FAsio
{
	FAsio() : resolver(context), socket(context) {}
	asio::error_code error_code;
	std::string exceptions;
	asio::io_context context;
	asio::ip::tcp::resolver resolver;
	asio::ip::tcp::resolver::results_type endpoints;
	asio::ip::tcp::socket socket;
};