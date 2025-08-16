/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#pragma once
#include "common.h"
#include "asio.generated.h"

USTRUCT(BlueprintType, Category="IP|Net|ErrorCode")
struct FErrorCode {
	GENERATED_BODY()
	FErrorCode() = default;
	explicit FErrorCode(const asio::error_code& error_code) {
		Value = error_code.value();
		Message = error_code.message().c_str();
	}

	UPROPERTY(BlueprintReadWrite, Category = "IP|ErrorCode")
	FString Message;

	UPROPERTY(BlueprintReadWrite, Category = "IP|ErrorCode")
	int Value;
};

USTRUCT(BlueprintType, Category="IP|Net|Endpoint")
struct FUdpEndpoint {
	GENERATED_BODY()
	FUdpEndpoint() = default;
	explicit FUdpEndpoint(udp::endpoint &ep) {
		Address = ep.address().to_string().c_str();
		Port = ep.port();
		Protocol = ep.protocol() == udp::v4() ? EProtocolType::V4 : EProtocolType::V6;
		Endpoint = ep;
	}

	UPROPERTY(BlueprintReadWrite, Category = "IP|Endpoint")
	FString Address;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Endpoint")
	int Port;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Endpoint")
	EProtocolType Protocol;

	udp::endpoint Endpoint;
};

USTRUCT(BlueprintType, Category="IP|Net|Endpoint")
struct FTcpEndpoint {
	GENERATED_BODY()
	FTcpEndpoint() = default;
	explicit FTcpEndpoint(tcp::endpoint &ep) {
		Address = ep.address().to_string().c_str();
		Port = ep.port();
		Protocol = ep.protocol() == tcp::v4() ? EProtocolType::V4 : EProtocolType::V6;
		Endpoint = ep;
	}

	UPROPERTY(BlueprintReadWrite, Category = "IP|Endpoint")
	FString Address;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Endpoint")
	int Port;

	UPROPERTY(BlueprintReadWrite, Category = "IP|Endpoint")
	EProtocolType Protocol;

	tcp::endpoint Endpoint;
};