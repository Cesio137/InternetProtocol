// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/Net/Commons.h"
#include "Message.generated.h"

USTRUCT(Blueprintable, Category = "IP")
struct FRequest
{
	GENERATED_BODY()
	TMap<FString, FString> params;
	EVerb verb = EVerb::GET;
	FString path = "/";
	FString version = "2.0";
	TMap<FString, FString> headers;
	FString body;
};