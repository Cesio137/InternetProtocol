// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnumLibrary.generated.h"

/**
 * 
 */

UENUM(BlueprintType)
enum ERepCondition
{
    Server_Only = 0 UMETA(DisplayName = "Server Only"),
    Owner_Only =  0 UMETA(DisplayName = "Owner Only"),
    Skip_Owner =  0 UMETA(DisplayName = "Skip Owner"),
};