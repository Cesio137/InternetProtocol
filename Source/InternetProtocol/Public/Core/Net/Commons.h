// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
THIRD_PARTY_INCLUDES_START
#include "asio.hpp"
THIRD_PARTY_INCLUDES_END
#include "Commons.generated.h"

UENUM(Blueprintable, Category = "IP")
enum class EVerb : uint8
{
	GET =       0,
	POST =      1,
	PUT =       2,
	PATCH =     3,
	DEL =       4,
	COPY =      5,
	HEAD =      6,
	OPTIONS =   7,
	LOCK =      8,
	UNLOCK =    9,
	PROPFIND = 10
};

USTRUCT(Blueprintable, Category = "IP")
struct FAsio
{
	GENERATED_BODY()
	FAsio() : resolver(context), socket(context) {}
	asio::error_code error_code;
	asio::io_context context;
	asio::ip::tcp::resolver resolver;
	asio::ip::tcp::resolver::results_type endpoints;
	asio::ip::tcp::socket socket;

	FAsio(const FAsio& asio) : resolver(context), socket(context)
	{
		error_code = asio.error_code;
		endpoints = asio.endpoints;
	}

	FAsio& operator=(const FAsio& asio)
	{
		if (this != &asio)
		{
			error_code = asio.error_code;
			endpoints = asio.endpoints;
		}
		return *this;
	}
};
