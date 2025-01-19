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
#include <vector>
#include <random>
#include <map>
#include <cstdint>
#include <algorithm>
#include <iostream>

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
#include "Async/Async.h"
#include "Logging/LogMacros.h"
#include "Library/InternetProtocolEnumLibrary.h"
#include "Commons.generated.h"

DEFINE_LOG_CATEGORY_STATIC(LogAsio, Log, All);
template <typename Exception>
inline void asio::detail::throw_exception(const Exception& e)
{
	UE_LOG(LogAsio, Error, TEXT("<ASIO ERROR>"));
	UE_LOG(LogAsio, Warning, TEXT("%hs"), e.what());
	UE_LOG(LogAsio, Error, TEXT("<ASIO ERROR/>"));
}

static TUniquePtr<asio::thread_pool> ThreadPool = nullptr;
static asio::thread_pool& GetThreadPool()
{
	if (!ThreadPool.IsValid())
	{
		ThreadPool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	}
	return *ThreadPool.Get();
}

USTRUCT(Blueprintable, Category = "IP")
struct FAsioTcpClient {
	GENERATED_BODY()
	FAsioTcpClient() : resolver(context),
				 socket(context) {
	}

	FAsioTcpClient(const FAsioTcpClient &asio) : resolver(context),
									 socket(context) {
		endpoints = asio.endpoints;
	}

	FAsioTcpClient &operator=(const FAsioTcpClient &asio) {
		if (this != &asio) {
			endpoints = asio.endpoints;
		}
		return *this;
	}

	asio::io_context context;
	asio::ip::tcp::resolver resolver;
	asio::ip::tcp::resolver::results_type endpoints;
	asio::ip::tcp::socket socket;
};

USTRUCT(Blueprintable, Category = "IP")
struct FAsioTcpSslClient {
	GENERATED_BODY()
	FAsioTcpSslClient()
		: context(),
		  ssl_context(asio::ssl::context::tlsv13_client),
		  ssl_socket(context, ssl_context),
		  resolver(context) {
		ssl_context.set_verify_mode(asio::ssl::verify_peer);
		ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(context, ssl_context);
	}

	FAsioTcpSslClient(const FAsioTcpSslClient &asio)
		: ssl_context(asio::ssl::context::tlsv13_client),
		  ssl_socket(context, ssl_context),
		  resolver(context),
		  endpoints(asio.endpoints) {
		ssl_context.set_verify_mode(asio::ssl::verify_peer);
		ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(context, ssl_context);
	}

	FAsioTcpSslClient &operator=(const FAsioTcpSslClient &asio) {
		if (this != &asio) {
			ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(context, ssl_context);
			endpoints = asio.endpoints;
		}
		return *this;
	}

	asio::io_context context;
	asio::ssl::context ssl_context;
	asio::ip::tcp::resolver resolver;
	asio::ip::tcp::resolver::results_type endpoints;
	asio::ssl::stream<asio::ip::tcp::socket> ssl_socket;
};

USTRUCT(BlueprintType, Category = "IP")
struct FAsioUdpClient {
	GENERATED_BODY()
	FAsioUdpClient() : socket(context),
				 resolver(context) {
	}

	FAsioUdpClient(const FAsioUdpClient &asio) : resolver(context),
									 socket(context),
									 endpoints(asio.endpoints) {
	}

	FAsioUdpClient &operator=(const FAsioUdpClient &asio) {
		if (this != &asio) {
			endpoints = asio.endpoints;
		}
		return *this;
	}

	asio::io_context context;
	asio::ip::udp::socket socket;
	asio::ip::udp::endpoint endpoints;
	asio::ip::udp::resolver resolver;
};

/*HTTP REQUEST*/
const TMap<EMethod, FString> RequestMethod = {
	{EMethod::DEL, "DELETE"}, {EMethod::GET, "GET"},
	{EMethod::HEAD, "HEAD"}, {EMethod::OPTIONS, "OPTIONS"},
	{EMethod::PATCH, "PATCH"}, {EMethod::POST, "POST"},
	{EMethod::PUT, "PUT"}, {EMethod::TRACE, "TRACE"},
};

/*HTTP RESPONSE*/
const TMap<int, FString> ResponseStatusCode = {
        // 1xx Informational
        {100, "Continue"},
        {101, "Switching Protocols"},
        {102, "Processing"},
        {103, "Early Hints"},

        // 2xx Success
        {200, "OK"},
        {201, "Created"},
        {202, "Accepted"},
        {203, "Non-Authoritative Information"},
        {204, "No Content"},
        {205, "Reset Content"},
        {206, "Partial Content"},
        {207, "Multi-Status"},
        {208, "Already Reported"},
        {226, "IM Used"},

        // 3xx Redirection
        {300, "Multiple Choices"},
        {301, "Moved Permanently"},
        {302, "Found"},
        {303, "See Other"},
        {304, "Not Modified"},
        {305, "Use Proxy"},
        {306, "Switch Proxy"},
        {307, "Temporary Redirect"},
        {308, "Permanent Redirect"},

        // 4xx Client Error
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {402, "Payment Required"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {406, "Not Acceptable"},
        {407, "Proxy Authentication Required"},
        {408, "Request Timeout"},
        {409, "Conflict"},
        {410, "Gone"},
        {411, "Length Required"},
        {412, "Precondition Failed"},
        {413, "Payload Too Large"},
        {414, "URI Too Long"},
        {415, "Unsupported Media Type"},
        {416, "Range Not Satisfiable"},
        {417, "Expectation Failed"},
        {418, "I'm a teapot"},
        {421, "Misdirected Request"},
        {422, "Unprocessable Entity"},
        {423, "Locked"},
        {424, "Failed Dependency"},
        {425, "Too Early"},
        {426, "Upgrade Required"},
        {428, "Precondition Required"},
        {429, "Too Many Requests"},
        {431, "Request Header Fields Too Large"},
        {451, "Unavailable For Legal Reasons"},

        // 5xx Server Error
        {500, "Internal Server Error"},
        {501, "Not Implemented"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"},
        {504, "Gateway Timeout"},
        {505, "HTTP Version Not Supported"},
        {506, "Variant Also Negotiates"},
        {507, "Insufficient Storage"},
        {508, "Loop Detected"},
        {510, "Not Extended"},
        {511, "Network Authentication Required"}
    };