#pragma once

#include "CoreMinimal.h"
#include "InternetProtocolEnumLibrary.generated.h"

/*HTTP*/
UENUM(Blueprintable, Category = "IP||HTTP")
enum class EVerb : uint8
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