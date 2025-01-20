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

#define UI UI_ST
THIRD_PARTY_INCLUDES_START

#if PLATFORM_WINDOWS
#include "Windows/PreWindowsApi.h"
#endif

#define ASIO_STANDALONE
#define ASIO_NO_DEPRECATED
#define ASIO_NO_EXCEPTIONS
#include <asio.hpp>
#include <asio/ssl.hpp>

#if PLATFORM_WINDOWS
#include "Windows/PostWindowsApi.h"
#endif

THIRD_PARTY_INCLUDES_END
#undef UI
#include "CoreMinimal.h"
#include "Library/InternetProtocolEnumLibrary.h"
#include "InternetProtocolStructLibrary.generated.h"

/*ERROR CODE*/
USTRUCT(BlueprintType, Category = "IP")
struct FErrorCode
{
	GENERATED_BODY()
	FErrorCode& operator=(const asio::error_code& error_code)
	{
		Value = error_code.value();
		Message = error_code.message().c_str();
		return *this;
	}
	FErrorCode() = default;
	FErrorCode(const asio::error_code& error_code)
	{
		Value = error_code.value();
		Message = error_code.message().c_str();
	}

	UPROPERTY(BlueprintReadWrite, Category = "IP||Error Code")
	int Value;

	UPROPERTY(BlueprintReadWrite, Category = "IP||Error Code")
	FString Message;
};

/*ADDRESS*/
USTRUCT(BlueprintType, Category = "IP")
struct FAddress
{
	GENERATED_BODY()
	FAddress() = default;
	FAddress(const asio::ip::address& address)
	{
		Adress = address.to_string().c_str();
		IsV4 = address.is_v4();
		IsV6 = address.is_v6();
	}

	FAddress& operator=(const asio::ip::address& address)
	{
		Adress = address.to_string().c_str();
		IsV4 = address.is_v4();
		IsV6 = address.is_v6();
		return *this;
	}
	
	UPROPERTY(BlueprintReadWrite, Category = "IP||Adress")
	FString Adress;

	UPROPERTY(BlueprintReadWrite, Category = "IP||Adress")
	bool IsV4;

	UPROPERTY(BlueprintReadWrite, Category = "IP||Adress")
	bool IsV6;
};

/*TCP*/
USTRUCT(BlueprintType, Category = "IP")
struct FTCPEndpoint
{
	GENERATED_BODY()
	FTCPEndpoint() = default;
	FTCPEndpoint(const asio::ip::tcp::endpoint& endpoint) : Endpoint(&endpoint)
	{
	}

	FTCPEndpoint& operator=(const asio::ip::tcp::endpoint& endpoint)
	{
		if (Endpoint == &endpoint)
			return *this;
		Endpoint = &endpoint;
		return *this;
	}

	const asio::ip::tcp::endpoint* Endpoint;
};

USTRUCT(BlueprintType, Category = "IP")
struct FTCPSocket
{
	GENERATED_BODY()
	FTCPSocket() = default;
	FTCPSocket(const asio::ip::tcp::socket& socket)
		: Socket(&socket)
	{
	}
	FTCPSocket& operator=(const asio::ip::tcp::socket& socket)
	{
		if (Socket == &socket)
			return *this;
		Socket = &socket;
		return *this;
	}

	const asio::ip::tcp::socket* Socket;
};

USTRUCT(BlueprintType, Category = "IP")
struct FTCPSslSocket
{
	GENERATED_BODY()
	FTCPSslSocket() = default;
	FTCPSslSocket(const asio::ssl::stream<asio::ip::tcp::socket>& socket)
		: SslSocket(&socket)
	{
	}
	FTCPSslSocket& operator=(const asio::ssl::stream<asio::ip::tcp::socket>& socket)
	{
		if (SslSocket == &socket)
			return *this;
		SslSocket = &socket;
		return *this;
	}

	const asio::ssl::stream<asio::ip::tcp::socket>* SslSocket;
};

USTRUCT(BlueprintType, Category = "IP")
struct FTCPSslNextLayer
{
	GENERATED_BODY()
	FTCPSslNextLayer() = default;
	FTCPSslNextLayer(const asio::ssl::stream<asio::ip::tcp::socket>::next_layer_type& socket)
		: SslNextLayer(&socket)
	{
	}
	FTCPSslNextLayer& operator=(const asio::ssl::stream<asio::ip::tcp::socket>::next_layer_type& socket)
	{
		if (SslNextLayer == &socket)
			return *this;
		SslNextLayer = &socket;
		return *this;
	}

	const asio::ssl::stream<asio::ip::tcp::socket>::next_layer_type* SslNextLayer;
};

USTRUCT(BlueprintType, Category = "IP")
struct FTCPSslLowestLayer
{
	GENERATED_BODY()
	FTCPSslLowestLayer() = default;
	FTCPSslLowestLayer(const asio::ssl::stream<asio::ip::tcp::socket>::lowest_layer_type& socket)
		: SslLowestLayer(&socket)
	{
	}
	FTCPSslLowestLayer& operator=(const asio::ssl::stream<asio::ip::tcp::socket>::lowest_layer_type& socket)
	{
		if (SslLowestLayer == &socket)
			return *this;
		SslLowestLayer = &socket;
		return *this;
	}

	const asio::ssl::stream<asio::ip::tcp::socket>::lowest_layer_type* SslLowestLayer;
};

/*SSL Context*/
USTRUCT(BlueprintType, Category = "IP")
struct FSslContext
{
	GENERATED_BODY()
	FSslContext() = default;
	FSslContext(asio::ssl::context& ex)
		: SslContext(&ex)
	{
	}
	FSslContext& operator=(asio::ssl::context& ex)
	{
		if (SslContext == &ex)
			return *this;
		SslContext = &ex;
		return *this;
	}

	asio::ssl::context* SslContext;
};

/*UDP*/
USTRUCT(BlueprintType, Category = "IP")
struct FUDPEndpoint
{
	GENERATED_BODY()
	FUDPEndpoint() = default;
	FUDPEndpoint(const asio::ip::udp::endpoint& endpoint) : Endpoint(&endpoint)
	{
	}

	FUDPEndpoint& operator=(const asio::ip::udp::endpoint& endpoint)
	{
		if (Endpoint == &endpoint)
			return *this;
		Endpoint = &endpoint;
		return *this;
	}
	bool operator==(const FUDPEndpoint& endpoint) const
	{
		if (this == &endpoint)
			return true;
		return false;
	}
	bool operator!=(const FUDPEndpoint& endpoint) const
	{
		if (this != &endpoint)
			return true;
		return false;
	}

	const asio::ip::udp::endpoint* Endpoint;
};

USTRUCT(BlueprintType, Category = "IP")
struct FUDPSocket
{
	GENERATED_BODY()
	FUDPSocket() = default;
	FUDPSocket(const asio::ip::udp::socket& socket)
		: Socket(&socket)
	{
	}
	FUDPSocket& operator=(const asio::ip::udp::socket& socket)
	{
		if (Socket == &socket)
			return *this;
		Socket = &socket;
		return *this;
	}

	const asio::ip::udp::socket* Socket;
};

/*HTTP*/
USTRUCT(BlueprintType, Category = "IP")
struct FClientRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	TMap<FString, FString> Params;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	EMethod Method = EMethod::GET;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString Path = "/";
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString Version = "1.1";
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	TMap<FString, FString> Headers;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString Body;
};

USTRUCT(BlueprintType, Category = "IP")
struct FClientResponse
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	TMap<FString, FString> Headers;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString Body;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	int ContentLength;
};

/*WEBSOCKET*/
USTRUCT(BlueprintType, Category = "IP")
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

USTRUCT(BlueprintType, Category = "IP")
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
