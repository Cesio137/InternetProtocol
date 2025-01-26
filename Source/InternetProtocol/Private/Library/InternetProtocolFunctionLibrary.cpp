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

#include "Library/InternetProtocolFunctionLibrary.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include <typeinfo>

FString UInternetProtocolFunctionLibrary::BufferToString(const TArray<uint8>& value)
{
	FUTF8ToTCHAR utf8_char(reinterpret_cast<const ANSICHAR*>(value.GetData()), value.Num());
	FString str(utf8_char.Length(), utf8_char.Get());
	return str;
}

TArray<uint8> UInternetProtocolFunctionLibrary::BoolToByteArray(bool value)
{
	bool boolean = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << boolean;
	return ByteArray;
}

bool UInternetProtocolFunctionLibrary::ByteArrayToBoolean(const TArray<uint8>& ByteArray)
{
	bool boolean;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << boolean;
	return boolean;
}

TArray<uint8> UInternetProtocolFunctionLibrary::IntToByteArray(const int& value)
{
	int integer = value;
	TArray<uint8> ByteArray;
	FMemoryWriter MemoryReader(ByteArray);
	MemoryReader << integer;
	return ByteArray;
}

int UInternetProtocolFunctionLibrary::ByteArrayToInt(const TArray<uint8>& ByteArray)
{
	int integer;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << integer;
	return integer;
}

TArray<uint8> UInternetProtocolFunctionLibrary::FloatToByteArray(const float& value)
{
	float f = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << f;
	return ByteArray;
}

float UInternetProtocolFunctionLibrary::ByteArrayToFloat(const TArray<uint8>& ByteArray)
{
	float f;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << f;
	return f;
}

TArray<uint8> UInternetProtocolFunctionLibrary::FVectorToByteArray(const FVector& value)
{
	FVector Vector = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << Vector;
	return ByteArray;
}

FVector UInternetProtocolFunctionLibrary::ByteArrayToFVector(const TArray<uint8>& ByteArray)
{
	FVector Vector;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Vector;
	return Vector;
}

TArray<uint8> UInternetProtocolFunctionLibrary::FRotatorToByteArray(const FRotator& value)
{
	FRotator Rotator = value;
	TArray<uint8> ByteArray;
	FMemoryWriter Writer(ByteArray);
	Writer << Rotator;
	return ByteArray;
}

FRotator UInternetProtocolFunctionLibrary::ByteArrayToFRotator(const TArray<uint8>& ByteArray)
{
	FRotator Rotator;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Rotator;
	return Rotator;
}

TArray<uint8> UInternetProtocolFunctionLibrary::FTransformToByteArray(const FTransform& value)
{
	FTransform Transform = value;
	TArray<uint8> ByteArray;
	FMemoryWriter MemoryReader(ByteArray);
	MemoryReader << Transform;
	return ByteArray;
}

FTransform UInternetProtocolFunctionLibrary::ByteArrayToFTransform(const TArray<uint8>& ByteArray)
{
	FTransform Transform;
	FMemoryReader MemoryReader(ByteArray);
	MemoryReader << Transform;
	return Transform;
}

TArray<FString> UHttpFunctionLibrary::DeserializeHeaderLine(const FString& value)
{
	TArray<FString> values;
	value.ParseIntoArray(values, TEXT(";"), true);
	for (FString& str : values)
	{
		FString Result = str;
		Result.TrimStartAndEndInline();
		str = Result;
	}

	return values;
}

void UHttpFunctionLibrary::ClearRequest(FClientRequest& request)
{
	request.Params.Empty();
	request.Method = EMethod::GET;
	request.Path = "/";
	request.Version = "1.1";
	request.Headers.Empty();
	request.Body.Empty();
}

void UHttpFunctionLibrary::ServerAppendHeader(FServerRequest& request, const FString& headerline)
{
	int32 Pos;
	if (headerline.FindChar(TEXT(':'), Pos))
	{
		FString key = TrimWhitespace(headerline.Left(Pos));
		FString value = TrimWhitespace(headerline.Mid(Pos + 1));
		request.Headers.Add(key, value);
	}
}

void UHttpFunctionLibrary::ServerSetBody(FServerRequest& response, const FString& value)
{
	if (value.IsEmpty()) return;
	response.Body = value;
}

void UHttpFunctionLibrary::ServerClearResponse(FServerResponse& response)
{
	response.Headers.Empty();
	response.Body.Empty();
}

void UHttpFunctionLibrary::ClientAppendHeader(FClientResponse& response, const FString& headerline)
{
	int32 Pos;
	if (headerline.FindChar(TEXT(':'), Pos))
	{
		FString key = TrimWhitespace(headerline.Left(Pos));
		FString value = TrimWhitespace(headerline.Mid(Pos + 1));
		if (key == "Content-Length")
		{
			response.ContentLength = FCString::Atoi(*value);
			return;
		}
		response.Headers.Add(key, value);
	}
}

void UHttpFunctionLibrary::ClientClearResponse(FClientResponse& response)
{
	response.Headers.Empty();
	response.ContentLength = 0;
	response.Body.Empty();
}

void UHttpFunctionLibrary::ClientSetBody(FClientResponse& response, const FString& value)
{
	if (value.IsEmpty()) return;
	response.Body = value;
}

void UHttpFunctionLibrary::ClientAppendBody(FClientResponse& response, const FString& value)
{
	response.Body.Append(value);
}

FString UHttpFunctionLibrary::TrimWhitespace(const FString& str)
{
	FString Result = str;
	Result.TrimStartAndEndInline();
	return Result;
}

int UUDPFunctionLibrary::Port(const FUDPEndpoint& endpoint)
{
	return endpoint.RawPtr->port();
}

FAddress UUDPFunctionLibrary::Address(const FUDPEndpoint& endpoint)
{
	return endpoint.RawPtr->address();
}

bool UUDPFunctionLibrary::IsOpen(const FUDPSocket& socket)
{
	return socket.RawPtr->is_open();
}

FUDPEndpoint UUDPFunctionLibrary::GetRemoteEndpoint(const FUDPSocket& socket)
{
	return socket.RawPtr->remote_endpoint();
}

FUDPEndpoint UUDPFunctionLibrary::GetLocalEndpoint(const FUDPSocket& socket)
{
	return socket.RawPtr->local_endpoint();
}

bool UTCPAcceptorFunctionLibrary::IsOpen(const FTCPAcceptor& acceptor)
{
	return acceptor.RawPtr->is_open();
}

FTCPEndpoint UTCPAcceptorFunctionLibrary::GetLocalEndpoint(const FTCPAcceptor& acceptor)
{
	return acceptor.RawPtr->local_endpoint();
}

int UTCPFunctionLibrary::Port(const FTCPEndpoint& endpoint)
{
	return endpoint.RawPtr->port();
}

FAddress UTCPFunctionLibrary::Address(const FTCPEndpoint& endpoint)
{
	return endpoint.RawPtr->address();
}

bool UTCPFunctionLibrary::IsOpen(const FTCPSocket& socket)
{
	return socket.RawPtr->is_open();
}

FTCPEndpoint UTCPFunctionLibrary::GetRemoteEndpoint(const FTCPSocket& socket)
{
	return socket.RawPtr->remote_endpoint();
}

FTCPEndpoint UTCPFunctionLibrary::GetLocalEndpoint(const FTCPSocket& socket)
{
	return socket.RawPtr->local_endpoint();
}

void UTCPSslContextFunctionLibrary::SetOptions(FSslContext& context, const ESslVerifyMode options)
{
	context.RawPtr->set_options(static_cast<asio::ssl::context_base::options>(options));
}

void UTCPSslContextFunctionLibrary::ClearOptions(FSslContext& context, const ESslVerifyMode options)
{
	context.RawPtr->clear_options(static_cast<asio::ssl::context_base::options>(options));
}

bool UTCPSslNextLayerFunctionLibrary::IsOpen(const FTCPSslNextLayer& next_layer)
{
	return next_layer.RawPtr->is_open();
}

FTCPEndpoint UTCPSslNextLayerFunctionLibrary::GetRemoteEndpoint(const FTCPSslNextLayer& socket)
{
	return socket.RawPtr->remote_endpoint();
}

FTCPEndpoint UTCPSslNextLayerFunctionLibrary::GetLocalEndpoint(const FTCPSslNextLayer& socket)
{
	return socket.RawPtr->local_endpoint();
}

bool UTCPSslLowestLayerFunctionLibrary::IsOpen(const FTCPSslLowestLayer& lowest_layer)
{
	return lowest_layer.RawPtr->is_open();
}

FTCPEndpoint UTCPSslLowestLayerFunctionLibrary::GetRemoteEndpoint(const FTCPSslLowestLayer& lowest_layer)
{
	return lowest_layer.RawPtr->remote_endpoint();
}

FTCPEndpoint UTCPSslLowestLayerFunctionLibrary::GetLocalEndpoint(const FTCPSslLowestLayer& lowest_layer)
{
	return lowest_layer.RawPtr->local_endpoint();
}

FTCPSslNextLayer UTCPSslFunctionLibrary::NextLayer(const FTCPSslSocket& ssl_socket)
{
	return ssl_socket.RawPtr->next_layer();
}

FTCPSslLowestLayer UTCPSslFunctionLibrary::LowestLayer(const FTCPSslSocket& ssl_socket)
{
	return ssl_socket.RawPtr->lowest_layer();
}
