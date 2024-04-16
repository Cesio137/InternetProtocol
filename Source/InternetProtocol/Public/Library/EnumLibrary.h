// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnumLibrary.generated.h"

/**
 * 
 */

//Exec Pins
UENUM(Blueprintable)
enum EOutputExecPins
{
	Success,
	Failure,
};

//HTTP
UENUM(BlueprintType)
enum class EVerbMode : uint8
{
	GET,
	POST,
	PUT,
	PATCH,
	DEL UMETA(DisplayName = "DELETE"),
	COPY,
	HEAD,
	OPTIONS,
	LINK,
	UNLINK,
	LOCK,
	UNLOCK,
	PROPFIND,
	VIEW
};

UENUM(BlueprintType)
enum class ERequestStatus : uint8
{
	NotStarted,
	Processing,
	Failed,
	Failed_ConnectionError,
	Succeeded,
};