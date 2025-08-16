/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#define UI UI_ST
#include <set>
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
#include "common.generated.h"

using namespace asio::ip;

DEFINE_LOG_CATEGORY_STATIC(LogAsio, Log, All);

template <typename Exception>
inline void asio::detail::throw_exception(const Exception& e) {
	UE_LOG(LogAsio, Error, TEXT("<ASIO ERROR>"));
	UE_LOG(LogAsio, Warning, TEXT("%hs"), e.what());
	UE_LOG(LogAsio, Error, TEXT("<ASIO ERROR/>"));
}

INTERNETPROTOCOL_API inline extern asio::thread_pool& thread_pool() {
	const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
	static asio::thread_pool pool(threads);
	return pool;
}

UENUM(Blueprintable, Category="IP|ENUM|Common")
enum class EProtocolType: uint8 {
	V4 = 0,
	V6 = 1,
};

UENUM(Blueprintable, Category="IP|ENUM|Common")
enum class EVerifyMode: uint8 {
	None = 0x00,
	Verify_Peer = 0x01,
	Verify_Fail_If_No_Peer_Cert = 0x02,
	Verify_Client_Once = 0x04,
};

// HTTP
UENUM(Blueprintable, Category="IP|ENUM|Common")
enum class ERequestMethod : uint8 {
	UNKNOWN = 0,
	CONNECT = 1,
	DEL = 2,
	GET = 3,
	HEAD = 4,
	OPTIONS = 5,
	PATCH = 6,
	POST = 7,
	PUT = 8,
	TRACE = 9
};

typedef asio::ssl::context_base::file_format file_format_e;

USTRUCT(BlueprintType, Category="IP|STRUCT|Common")
struct FHttpRequest {
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "IP|Request")
	ERequestMethod Method = ERequestMethod::GET;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Request")
	FString Path = "/";

	UPROPERTY(BlueprintReadWrite, Category = "IP|Request")
	FString Version = "1.1";

	UPROPERTY(BlueprintReadWrite, Category = "IP|Request")
	TMap<FString, FString> Params;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Request")
	TMap<FString, FString> Headers;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Request")
	FString Body;
};

USTRUCT(BlueprintType, Category="IP|STRUCT|Common")
struct FHttpResponse {
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "IP|Response")
	int Status_Code = 200;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Response")
	FString Status_Message;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Response")
	FString Version = "1.1";

	UPROPERTY(BlueprintReadWrite, Category = "IP|Response")
	TMap<FString, FString> Headers;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Response")
	FString Body;
};

static TMap<int, FString> Response_Status = {
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

UENUM(Blueprintable, Category="IP|ENUM|Common")
enum class EOpcode: uint8 {
	NONE = 0x00,
	TEXT_FRAME = 0x01,
	BINARY_FRAME = 0x02,
	CLOSE_FRAME = 0x08,
	PING = 0x09,
	PONG = 0x0A
};

UENUM(Blueprintable, Category="IP|ENUM|Common")
enum class ERSV: uint8 {
	NONE = 0x00,
	RSV1 = 0x40,
	RSV2 = 0x20,
	RSV3 = 0x10
};

USTRUCT(BlueprintType, Category="IP|STRUCT|Common")
struct FDataframe {
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "IP|DataFrame")
	bool bFin = true;
	
	UPROPERTY(BlueprintReadWrite, Category = "IP|DataFrame")
	bool bRsv1 = false;

	UPROPERTY(BlueprintReadWrite, Category = "IP|DataFrame")
	bool bRsv2 = false;

	UPROPERTY(BlueprintReadWrite, Category = "IP|DataFrame")
	bool bRsv3 = false;

	UPROPERTY(BlueprintReadWrite, Category = "IP|DataFrame")
	bool bMask = true;
	
	EOpcode Opcode = EOpcode::TEXT_FRAME;
	size_t Length = 0;
	std::array<uint8_t, 4> masking_key{};
};

UENUM(Blueprintable, Category="IP|ENUM|Common")
enum class ECloseState: uint8  {
	CLOSED = 0,
	CLOSING = 1,
	OPEN = 2
};

// Client side
USTRUCT(BlueprintType, Category="IP|STRUCT|Common")
struct FClientBindOptions {
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "IP|ClientBindOptions")
	FString Address = "localhost";

	UPROPERTY(BlueprintReadWrite, Category = "IP|ClientBindOptions")
	FString Port = "8080";

	UPROPERTY(BlueprintReadWrite, Category = "IP|ClientBindOptions")
	EProtocolType Protocol = EProtocolType::V4;
};

// Server side
USTRUCT(BlueprintType, Category="IP|STRUCT|Common")
struct FServerBindOptions {
	GENERATED_BODY()
	FString Address = "localhost";
	int Port = 8080;
	EProtocolType Protocol = EProtocolType::V4;
	bool bReuse_Address = true;
};