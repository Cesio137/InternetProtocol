//© Nathan Miguel, 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnumLibrary.generated.h"

/**
 * 
 */

//Exec Pins
UENUM(Blueprintable, Category="IP")
enum EOutputExecPins
{
	Success,
	Failure,
};

//HTTP
UENUM(BlueprintType, Category = "IP")
enum class ERequestStatus : uint8
{
	NotStarted,
	Processing,
	Failed,
	Failed_ConnectionError,
	Succeeded,
};

UENUM(Blueprintable, Category = "IP")
enum class EHttpVerb : uint8
{
    GET =       0,
    POST =      1,
    PUT =       2,
    PATCH =     3,
    DEL =       4,
    COPY =      5,
    HEAD =      6,
    OPTIONS =   7,
    LOCK =      8,
    UNLOCK =    9,
    PROPFIND = 10
};