#pragma once

#include "CoreMinimal.h"
#include "InternetProtocolEnumLibrary.generated.h"

/*Exec Pin*/
UENUM(Blueprintable, Category="IP")
enum EOutputExecPins
{
	Success,
	Failure,
};

/*HTTP*/
UENUM(Blueprintable, Category = "IP||HTTP")
enum class EVerb : uint8
{
	GET = 0,
	POST = 1,
	PUT = 2,
	PATCH = 3,
	DEL = 4,
	COPY = 5,
	HEAD = 6,
	OPTIONS = 7,
	LOCK = 8,
	UNLOCK = 9,
	PROPFIND = 10
};

/*WEBSOCKET*/
UENUM(Blueprintable, Category = "IP||WEBSOCKET")
enum class EOpcode : uint8
{
	FRAME_CON = 0x00,
	TEXT_FRAME = 0x01,
	BINARY_FRAME = 0x02,
	NON_CONTROL_FRAMES = 0x03,
	CONNECTION_CLOSE = 0x08,
	PING = 0x09,
	PONG = 0x0A,
	FURTHER_FRAMES = 0x0B
};

UENUM(Blueprintable, Category = "IP||WEBSOCKET")
enum class ERSV : uint8
{
	RSV0 = 0x00,
	RSV1 = 0x40,
	RSV2 = 0x20,
	RSV3 = 0x10
};
