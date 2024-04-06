// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Json/JavaScriptObjectNotation.h"
#include "JsonSubsystem.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class INTERNETPROTOCOL_API UJsonSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Json")
	UJavaScriptObjectNotation* CreateJsonObject();
};
