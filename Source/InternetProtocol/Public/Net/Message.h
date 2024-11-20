/*
 * Copyright (c) 2023-2024 Nathan Miguel
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
#include "Library/InternetProtocolStructLibrary.h"
#include "Message.generated.h"

USTRUCT(Blueprintable, Category = "IP")
struct FUdpMessage
{
	GENERATED_BODY()
	FUdpMessage() { RawData.SetNum(1024); }
	~FUdpMessage()
	{
		if (RawData.Num() > 0)
			RawData.Empty();
	}
	UPROPERTY(BlueprintReadWrite, Category="IP||UDP")
	TArray<uint8> RawData;
	UPROPERTY(BlueprintReadWrite, Category="IP||UDP")
	int size = 0;
};

USTRUCT(Blueprintable, Category = "IP")
struct FTcpMessage
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="IP||TCP")
	TArray<uint8> RawData;
	UPROPERTY(BlueprintReadWrite, Category="IP||TCP")
	int size = 0;
};

USTRUCT(Blueprintable, Category = "IP")
struct FWsMessage
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="IP||Websocket")
	FDataFrame DataFrame;
	UPROPERTY(BlueprintReadWrite, Category="IP||Websocket")
	TArray<uint8> Payload;
};
