/*
 * Copyright (c) 2023-2025 Nathan Miguel
 *
 * InternetProtocol is free library: you can redistribute it and/or modify it under the terms
 * of the GNU Affero General Public License as published by the Free Software Foundation,
 * version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
*/

#pragma once

#include "CoreMinimal.h"
#include "InternetProtocolEnumLibrary.generated.h"

/*Exec Pin*/
UENUM(Blueprintable, Category="IP|ENUM|Exec Pins")
enum EOutputExecPins
{
	Success,
	Failure,
};

/*UDP*/
UENUM(BlueprintType, Category = "IP|ENUM|Protocol Type")
enum class EProtocolType : uint8
{
	V4 = 0,
	v6 = 1
};

/*HTTP*/
UENUM(Blueprintable, Category = "IP|ENUM|HTTP")
enum class EMethod : uint8
{
	DEL = 0,
	GET = 1,
	HEAD = 2,
	OPTIONS = 3,
	PATCH = 4,
	POST = 5,
	PUT = 6,
	TRACE = 7,
};

/*WEBSOCKET*/
UENUM(Blueprintable, Category = "IP|ENUM|Websocket")
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

UENUM(Blueprintable, Category = "IP|ENUM|Websocket")
enum class ERSV : uint8
{
	RSV0 = 0x00,
	RSV1 = 0x40,
	RSV2 = 0x20,
	RSV3 = 0x10
};

/*SSL*/
UENUM(Blueprintable, Category = "IP|ENUM|SSL")
enum class ESslVerifyMode : uint8
{
	VERIFY_NONE = 0,
	VERIFY_PEER = 1,
	VERIFY_FAIL_IF_NO_PEER = 2,
	VERIFY_CLIENT_ONCE = 4
};
