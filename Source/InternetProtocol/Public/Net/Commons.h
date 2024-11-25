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
#include "Async/Async.h"
#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include <vector>
#include <random>
#include <map>
#include <cstdint>
#include <algorithm>
#include <iostream>

#define ASIO_STANDALONE
#define ASIO_NO_DEPRECATED
#define ASIO_NO_EXCEPTIONS
#define ASIO_DISABLE_CXX11_MACROS
#include <asio.hpp>
#include <asio/ssl.hpp>

THIRD_PARTY_INCLUDES_END
#undef UI
#include "Commons.generated.h"

USTRUCT(Blueprintable, Category = "IP")
struct FAsioTcp
{
	GENERATED_BODY()
	FAsioTcp() : resolver(context), socket(context)
	{
	}

	asio::error_code error_code;
	asio::io_context context;
	asio::ip::tcp::resolver resolver;
	asio::ip::tcp::resolver::results_type endpoints;
	asio::ip::tcp::socket socket;
	uint8 attemps_fail = 0;

	FAsioTcp(const FAsioTcp& asio) : resolver(context), socket(context)
	{
		error_code = asio.error_code;
		endpoints = asio.endpoints;
	}

	FAsioTcp& operator=(const FAsioTcp& asio)
	{
		if (this != &asio)
		{
			error_code = asio.error_code;
			endpoints = asio.endpoints;
		}
		return *this;
	}
};

USTRUCT(Blueprintable, Category = "IP")
struct FAsioTcpSsl
{
	GENERATED_BODY()
	FAsioTcpSsl() : context(),
	          ssl_context(asio::ssl::context::sslv23),
			  resolver(context),
			  ssl_socket(context, ssl_context)
	{
		ssl_context.set_verify_mode(asio::ssl::verify_peer);
	}

	asio::error_code error_code;
	asio::io_context context;
	asio::ssl::context ssl_context;
	asio::ip::tcp::resolver resolver;
	asio::ip::tcp::resolver::results_type endpoints;
	asio::ssl::stream<asio::ip::tcp::socket> ssl_socket;
	uint8_t attemps_fail = 0;

	FAsioTcpSsl(const FAsioTcpSsl &asio)
			: context(),
			  ssl_context(asio::ssl::context::sslv23),
			  resolver(context),
			  ssl_socket(context, ssl_context) {
		ssl_context.set_verify_mode(asio::ssl::verify_peer);
		error_code = asio.error_code;
		endpoints = asio.endpoints;
	}

	FAsioTcpSsl &operator=(const FAsioTcpSsl &asio) {
		if (this != &asio) {
			ssl_context.set_verify_mode(asio::ssl::verify_peer);
			error_code = asio.error_code;
			endpoints = asio.endpoints;
		}
		return *this;
	}
};

USTRUCT(Blueprintable, Category = "IP")
struct FAsioUdp
{
	GENERATED_BODY()
	FAsioUdp() : socket(context), resolver(context)
	{
	}

	~FAsioUdp()
	{
		
	}

	asio::error_code error_code;
	asio::io_context context;
	asio::ip::udp::socket socket;
	asio::ip::udp::endpoint endpoints;
	asio::ip::udp::resolver resolver;
	uint8 attemps_fail = 0;

	FAsioUdp(const FAsioUdp& asio) : socket(context), resolver(context)
	{
		error_code = asio.error_code;
		endpoints = asio.endpoints;
	}

	FAsioUdp& operator=(const FAsioUdp& asio)
	{
		if (this != &asio)
		{
			error_code = asio.error_code;
			endpoints = asio.endpoints;
		}
		return *this;
	}
};
