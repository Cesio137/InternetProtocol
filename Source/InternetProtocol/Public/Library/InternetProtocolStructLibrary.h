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
#include "Library/InternetProtocolEnumLibrary.h"
THIRD_PARTY_INCLUDES_START
#include "asio.hpp"
THIRD_PARTY_INCLUDES_END
#include "InternetProtocolStructLibrary.generated.h"

/*HTTP*/
USTRUCT(Blueprintable, Category = "IP")
struct FRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	TMap<FString, FString> params;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	EVerb verb = EVerb::GET;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString path = "/";
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString version = "1.1";
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	TMap<FString, FString> headers;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString body;

	void clear()
	{
		params.Empty();
		verb = EVerb::GET;
		path = "/";
		version = "2.0";
		headers.Empty();
		body.Empty();
	}
};

USTRUCT(Blueprintable, Category = "IP")
struct FResponse
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	TMap<FString, FString> Headers;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString Content;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	int ContentLength;

	void appendHeader(const FString& headerline)
	{
		int32 Pos;
		if (headerline.FindChar(TEXT(':'), Pos))
		{
			FString key = trimWhitespace(headerline.Left(Pos));
			FString value = trimWhitespace(headerline.Mid(Pos + 1));
			if (key == "Content-Length")
			{
				ContentLength = FCString::Atoi(*value);
				return;
			}
			Headers.Add(key, value);
		}
	}

	void setContent(const FString& value)
	{
		if (value.IsEmpty())
		{
			return;
		}
		Content = value;
	}

	void appendContent(const FString& value)
	{
		Content.Append(value);
	}

	void clear()
	{
		Headers.Empty();
		ContentLength = 0;
		Content.Empty();
	}

private:
	FString trimWhitespace(const FString& str) const
	{
		FString Result = str;
		Result.TrimStartAndEndInline();
		return Result;
	}
};

/*WEBSOCKET*/
USTRUCT(Blueprintable, Category = "IP")
struct FDataFrame
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	bool fin = true;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	bool rsv1 = false;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	bool rsv2 = false;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	bool rsv3 = false;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	bool mask = true;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	EOpcode opcode = EOpcode::TEXT_FRAME;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	int length = 0;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	TArray<uint8> masking_key;
};

USTRUCT(Blueprintable, Category = "IP")
struct FHandShake
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString path = "chat";
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString version = "1.1";
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString Sec_WebSocket_Key = "dGhlIHNhbXBsZSBub25jZQ==";
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString origin = "client";
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString Sec_WebSocket_Protocol = "chat, superchat";
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString Sec_Websocket_Version = "13";
};
