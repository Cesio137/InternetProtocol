/** 
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "net/common.h"

/**
 * 
 */

std::array<uint8, 4> mask_gen();

FString encode_string_payload(const FString &payload, const FDataframe &dataframe);

TArray<uint8> encode_buffer_payload(const TArray<uint8> &payload, const FDataframe &dataframe);

bool decode_payload(const TArray<uint8> &buffer, TArray<uint8> &payload,
                               FDataframe &dataframe);
