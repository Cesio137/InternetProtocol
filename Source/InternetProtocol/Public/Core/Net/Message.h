// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/Net/Commons.h"
#include "Message.generated.h"

USTRUCT(Blueprintable, Category = "IP")
struct FTcpMessage
{
	GENERATED_BODY()
	FTcpMessage() { message.SetNum(8192); }
	TArray<char> message;
	UPROPERTY(BlueprintReadWrite, Category="IP||TCP")
	int size = 0;
	FString toString() const {
		return message.GetData();
	}
};

USTRUCT(Blueprintable, Category = "IP")
struct FUdpMessage
{
	GENERATED_BODY()
	FUdpMessage() { message.SetNum(1024); }
	TArray<char> message;
	UPROPERTY(BlueprintReadWrite, Category="IP||UDP")
	int size = 0;
	FString toString() const {
		return message.GetData();
	}
};