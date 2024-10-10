// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include <vector>
#include <random>
#include <map>
#include <cstdint>
#include <algorithm>

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE
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
	FAsioTcpSsl() : ssl_context(asio::ssl::context::sslv23),
			  context(),
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
			: ssl_context(asio::ssl::context::sslv23),
			  context(),
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
