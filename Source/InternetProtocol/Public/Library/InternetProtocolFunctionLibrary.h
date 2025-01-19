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

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Net/Message.h"
#include "InternetProtocolFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, Category = "IP")
class INTERNETPROTOCOL_API UInternetProtocolFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/*Binary functions*/

	// Convert buffer to string
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="IP||Buffer")
	static FString BufferToString(const TArray<uint8>& value);

	// Convert bool to byte array | Convert byte array to bool	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static TArray<uint8> BoolToByteArray(bool value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static bool ByteArrayToBoolean(const TArray<uint8>& ByteArray);

	// Convert int to byte array | Convert byte array to int
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static TArray<uint8> IntToByteArray(const int& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static int ByteArrayToInt(const TArray<uint8>& ByteArray);

	// Convert float to byte array | Convert byte array to float
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static TArray<uint8> FloatToByteArray(const float& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static float ByteArrayToFloat(const TArray<uint8>& ByteArray);

	// Convert FVector to byte array | Convert byte array to float
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static TArray<uint8> FVectorToByteArray(const FVector& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static FVector ByteArrayToFVector(const TArray<uint8>& ByteArray);

	// Convert FRotator to byte array | Convert byte array to float
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static TArray<uint8> FRotatorToByteArray(const FRotator& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static FRotator ByteArrayToFRotator(const TArray<uint8>& ByteArray);

	// Convert FRotator to byte array | Convert byte array to float
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static TArray<uint8> FTransformToByteArray(const FTransform& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Binary")
	static FTransform ByteArrayToFTransform(const TArray<uint8>& ByteArray);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP")
class INTERNETPROTOCOL_API UHttpFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/*Http functions*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="IP||HTTP||Response")
	static TArray<FString> DeserializeHeaderLine(const FString& value);

	/*Response functions*/
	static void ClearRequest(FClientRequest& request);
	
	/*Response functions*/
	static void AppendHeader(FClientResponse& response, const FString& headerline);
	static void ClearResponse(FClientResponse& response);
	static void SetBody(FClientResponse& response, const FString& value);
	static void AppendBody(FClientResponse& response, const FString& value);

private:
	static FString TrimWhitespace(const FString& str);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP")
class INTERNETPROTOCOL_API UUDPFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/*UDP functions*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Endpoint")
	static int Port(const FUDPEndpoint& endpoint);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Endpoint")
	static FAddress Address(const FUDPEndpoint& endpoint);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Socket")
	static bool IsOpen(const FUDPSocket& socket);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Socket")
	static FUDPEndpoint GetRemoteEndpoint(const FUDPSocket& socket);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||UDP||Socket")
	static FUDPEndpoint GetLocalEndpoint(const FUDPSocket& socket);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP")
class INTERNETPROTOCOL_API UTCPFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/*TCP functions*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Endpoint")
	static int Port(const FTCPEndpoint& endpoint);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Endpoint")
	static FAddress Address(const FTCPEndpoint& endpoint);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Socket")
	static bool IsOpen(const FTCPSocket& socket);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Socket")
	static FTCPEndpoint GetRemoteEndpoint(const FTCPSocket& socket);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Socket")
	static FTCPEndpoint GetLocalEndpoint(const FTCPSocket& socket);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP")
class INTERNETPROTOCOL_API UTCPSslNextLayerFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/*TCP functions*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP SSL||Next Layer||Endpoint")
	static bool IsOpen(const FTCPSslNextLayer& next_layer);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP SSL||Next Layer||Endpoint")
	static FTCPEndpoint GetRemoteEndpoint(const FTCPSslNextLayer& next_layer);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP SSL||Next Layer||Endpoint")
	static FTCPEndpoint GetLocalEndpoint(const FTCPSslNextLayer& next_layer);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP")
class INTERNETPROTOCOL_API UTCPSslLowestLayerFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/*TCP functions*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP SSL||Lowest Layer||Endpoint")
	static bool IsOpen(const FTCPSslLowestLayer& lowest_layer);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP SSL||Lowest Layer||Endpoint")
	static FTCPEndpoint GetRemoteEndpoint(const FTCPSslLowestLayer& lowest_layer);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP SSL||Lowest Layer||Endpoint")
	static FTCPEndpoint GetLocalEndpoint(const FTCPSslLowestLayer& lowest_layer);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP")
class INTERNETPROTOCOL_API UTCPSslFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/*TCP functions*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP SSL||Next layer")
	static FTCPSslNextLayer NextLayer(const FTCPSslSocket& ssl_socket);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP SSL||Lowest layer")
	static FTCPSslLowestLayer LowestLayer(const FTCPSslSocket& ssl_socket);
};
