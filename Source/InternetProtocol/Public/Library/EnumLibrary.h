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