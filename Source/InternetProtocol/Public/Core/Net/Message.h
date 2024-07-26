// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/Net/Commons.h"
#include "Library/InternetProtocolStructLibrary.h"
#include "Message.generated.h"

USTRUCT(Blueprintable, Category = "IP")
struct FUdpMessage
{
	GENERATED_BODY()
	FUdpMessage() { RawData.SetNum(1024); }
	UPROPERTY(BlueprintReadWrite, Category="IP||UDP")
	TArray<uint8> RawData;
	UPROPERTY(BlueprintReadWrite, Category="IP||UDP")
	int size = 0;
};

USTRUCT(Blueprintable, Category = "IP")
struct FTcpMessage
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="IP||TCP")
	TArray<uint8> RawData;
	UPROPERTY(BlueprintReadWrite, Category="IP||TCP")
	int size = 0;
};

USTRUCT(Blueprintable, Category = "IP")
struct FWsMessage
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="IP||Websocket")
	FDataFrame DataFrame;
	UPROPERTY(BlueprintReadWrite, Category="IP||Websocket")
	TArray<uint8> Payload;
};
