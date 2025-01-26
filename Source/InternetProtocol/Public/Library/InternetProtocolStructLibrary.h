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
USTRUCT(BlueprintType, Category = "IP|STRUCT")
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

	UPROPERTY(BlueprintReadWrite, Category = "IP|Error Code")
	int Value;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Error Code")
	FString Message;
};

/*ADDRESS*/
USTRUCT(BlueprintType, Category = "IP|STRUCT")
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
	
	UPROPERTY(BlueprintReadWrite, Category = "IP|Adress")
	FString Adress;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Adress")
	bool IsV4;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Adress")
	bool IsV6;
};

/*TCP*/
USTRUCT(BlueprintType, Category = "IP|STRUCT")
struct FTCPEndpoint
{
	GENERATED_BODY()
	FTCPEndpoint() = default;
	FTCPEndpoint(const asio::ip::tcp::endpoint& endpoint) : RawPtr(&endpoint)
	{
	}

	FTCPEndpoint& operator=(const asio::ip::tcp::endpoint& endpoint)
	{
		if (RawPtr == &endpoint)
			return *this;
		RawPtr = &endpoint;
		return *this;
	}

	const asio::ip::tcp::endpoint* RawPtr;
};

USTRUCT(BlueprintType, Category = "IP|STRUCT|TCP")
struct FTCPAcceptor
{
	GENERATED_BODY()
	FTCPAcceptor() = default;
	FTCPAcceptor(asio::ip::tcp::acceptor& acceptor)
		: RawPtr(&acceptor)
	{
	}
	FTCPAcceptor& operator=(asio::ip::tcp::acceptor& acceptor)
	{
		if (RawPtr == &acceptor)
			return *this;
		RawPtr = &acceptor;
		return *this;
	}

	asio::ip::tcp::acceptor* RawPtr;
};

USTRUCT(BlueprintType, Category = "IP|STRUCT|TCP")
struct FTCPSocket
{
	GENERATED_BODY()
	FTCPSocket() = default;
	FTCPSocket(const TSharedPtr<asio::ip::tcp::socket>& socket) : RawPtr(socket.Get()), SmartPtr(socket)
	{
	}
	FTCPSocket(asio::ip::tcp::socket* socket) : RawPtr(socket)
	{
	}
	FTCPSocket(asio::ip::tcp::socket& socket) : RawPtr(&socket)
	{
	}

	FTCPSocket& operator=(asio::ip::tcp::socket* socket)
	{
		if (RawPtr == socket)
			return *this;
		RawPtr = socket;
		return *this;
	}
	FTCPSocket& operator=(asio::ip::tcp::socket& socket)
	{
		if (RawPtr == &socket)
			return *this;
		RawPtr = &socket;
		return *this;
	}

	asio::ip::tcp::socket* RawPtr;
	TSharedPtr<asio::ip::tcp::socket> SmartPtr;
};

USTRUCT(BlueprintType, Category = "IP|STRUCT|SSL")
struct FTCPSslContext
{
	GENERATED_BODY()
	FTCPSslContext() = default;
	FTCPSslContext(const asio::ssl::context* context)
		: RawPtr(context)
	{
	}
	FTCPSslContext(const asio::ssl::context& context)
		: RawPtr(&context)
	{
	}
	FTCPSslContext& operator=(const asio::ssl::context& context)
	{
		if (RawPtr == &context)
			return *this;
		RawPtr = &context;
		return *this;
	}

	const asio::ssl::context* RawPtr;
};

USTRUCT(BlueprintType, Category = "IP|STRUCT|SSL|TCP")
struct FTCPSslSocket
{
	GENERATED_BODY()
	FTCPSslSocket() = default;
	FTCPSslSocket(const TSharedPtr<asio::ssl::stream<asio::ip::tcp::socket>>& socket)
		: SmartPtr(socket), RawPtr(socket.Get())
	{
	}
	FTCPSslSocket(asio::ssl::stream<asio::ip::tcp::socket>* socket)
		: RawPtr(socket)
	{
	}
	FTCPSslSocket(asio::ssl::stream<asio::ip::tcp::socket>& socket)
		: RawPtr(&socket)
	{
	}

	FTCPSslSocket& operator=(const TSharedPtr<asio::ssl::stream<asio::ip::tcp::socket>>& socket)
	{
		if (SmartPtr == socket && RawPtr == socket.Get())
			return *this;
		SmartPtr = socket;
		RawPtr = socket.Get();
		return *this;
	}
	FTCPSslSocket& operator=(asio::ssl::stream<asio::ip::tcp::socket>* socket)
	{
		if (RawPtr == socket)
			return *this;
		RawPtr = socket;
		return *this;
	}
	FTCPSslSocket& operator=(asio::ssl::stream<asio::ip::tcp::socket>& socket)
	{
		if (RawPtr == &socket)
			return *this;
		RawPtr = &socket;
		return *this;
	}

	asio::ssl::stream<asio::ip::tcp::socket>* RawPtr;
	TSharedPtr<asio::ssl::stream<asio::ip::tcp::socket>> SmartPtr;
};

USTRUCT(BlueprintType, Category = "IP|STRUCT|SSL|TCP")
struct FTCPSslNextLayer
{
	GENERATED_BODY()
	FTCPSslNextLayer() = default;
	FTCPSslNextLayer(const asio::ssl::stream<asio::ip::tcp::socket>::next_layer_type& socket)
		: RawPtr(&socket)
	{
	}
	FTCPSslNextLayer& operator=(const asio::ssl::stream<asio::ip::tcp::socket>::next_layer_type& socket)
	{
		if (RawPtr == &socket)
			return *this;
		RawPtr = &socket;
		return *this;
	}

	const asio::ssl::stream<asio::ip::tcp::socket>::next_layer_type* RawPtr;
};

USTRUCT(BlueprintType, Category = "IP|STRUCT|SSL|TCP")
struct FTCPSslLowestLayer
{
	GENERATED_BODY()
	FTCPSslLowestLayer() = default;
	FTCPSslLowestLayer(const asio::ssl::stream<asio::ip::tcp::socket>::lowest_layer_type& socket)
		: RawPtr(&socket)
	{
	}
	FTCPSslLowestLayer& operator=(const asio::ssl::stream<asio::ip::tcp::socket>::lowest_layer_type& socket)
	{
		if (RawPtr == &socket)
			return *this;
		RawPtr = &socket;
		return *this;
	}

	const asio::ssl::stream<asio::ip::tcp::socket>::lowest_layer_type* RawPtr;
};

/*SSL Context*/
USTRUCT(BlueprintType, Category = "IP|STRUCT|SSL")
struct FSslContext
{
	GENERATED_BODY()
	FSslContext() = default;
	FSslContext(asio::ssl::context& ex)
		: RawPtr(&ex)
	{
	}
	FSslContext& operator=(asio::ssl::context& ex)
	{
		if (RawPtr == &ex)
			return *this;
		RawPtr = &ex;
		return *this;
	}

	asio::ssl::context* RawPtr;
};

/*UDP*/
USTRUCT(BlueprintType, Category = "IP|STRUCT|UDP")
struct FUDPEndpoint
{
	GENERATED_BODY()
	FUDPEndpoint() = default;
	FUDPEndpoint(const asio::ip::udp::endpoint& endpoint) : RawPtr(&endpoint)
	{
	}

	FUDPEndpoint& operator=(const asio::ip::udp::endpoint& endpoint)
	{
		if (RawPtr == &endpoint)
			return *this;
		RawPtr = &endpoint;
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

	const asio::ip::udp::endpoint* RawPtr;
};

USTRUCT(BlueprintType, Category = "IP|STRUCT|UDP")
struct FUDPSocket
{
	GENERATED_BODY()
	FUDPSocket() = default;
	FUDPSocket(const asio::ip::udp::socket& socket)
		: RawPtr(&socket)
	{
	}
	FUDPSocket& operator=(const asio::ip::udp::socket& socket)
	{
		if (RawPtr == &socket)
			return *this;
		RawPtr = &socket;
		return *this;
	}

	const asio::ip::udp::socket* RawPtr;
};

/*HTTP*/
USTRUCT(BlueprintType, Category = "IP|STRUCT|HTTP")
struct FServerRequest {
	GENERATED_BODY()
	~FServerRequest() {
		Headers.Empty();
	}

	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	EMethod Method = EMethod::GET;
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	FString Path = "/";
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	FString Version = "1.1";
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	TMap<FString, FString> Headers;
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	FString Body;
};

USTRUCT(BlueprintType, Category = "IP|STRUCT|HTTP")
struct FServerResponse {
	GENERATED_BODY()
	~FServerResponse() {
		Headers.Empty();
	}

	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	FString Version = "1.1";
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	TMap<FString, FString> Headers;
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	FString Body;
};

USTRUCT(BlueprintType, Category = "IP|STRUCT|HTTP")
struct FClientRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	TMap<FString, FString> Params;
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	EMethod Method = EMethod::GET;
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	FString Path = "/";
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	FString Version = "1.1";
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	TMap<FString, FString> Headers;
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	FString Body;
};

USTRUCT(BlueprintType, Category = "IP|STRUCT|HTTP")
struct FClientResponse
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	TMap<FString, FString> Headers;
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	FString Body;
	UPROPERTY(BlueprintReadWrite, Category="IP|HTTP")
	int ContentLength;
};

/*WEBSOCKET*/
USTRUCT(BlueprintType, Category = "IP|STRUCT|Websocket")
struct FDataFrame
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	bool fin = true;
	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	bool rsv1 = false;
	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	bool rsv2 = false;
	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	bool rsv3 = false;
	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	bool mask = true;
	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	EOpcode opcode = EOpcode::TEXT_FRAME;
	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	int length = 0;
	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	TArray<uint8> masking_key;
};

USTRUCT(BlueprintType, Category = "IP|STRUCT|Websocket")
struct FHandShake
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	FString path = "chat";
	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	FString version = "1.1";
	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	FString Sec_WebSocket_Key = "dGhlIHNhbXBsZSBub25jZQ==";
	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	FString origin = "client";
	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	FString Sec_WebSocket_Protocol = "chat, superchat";
	UPROPERTY(BlueprintReadWrite, Category="IP|Websocket")
	FString Sec_Websocket_Version = "13";
};
