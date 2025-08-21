/** 
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "net/common.h"

/**
 * 
 */

// SHA1
static uint32_t h0 = 0x67452301;
static uint32_t h1 = 0xEFCDAB89;
static uint32_t h2 = 0x98BADCFE;
static uint32_t h3 = 0x10325476;
static uint32_t h4 = 0xC3D2E1F0;

// Accept key
static FString magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Base encoded
inline const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::array<uint8, 20> sha1(const FString &input);

FString base64_encode(const uint8 *input, size_t length);

FString generate_accept_key(const FString &sec_websocket_key);

bool validate_handshake_request(const FHttpRequest &req_handshake, FHttpResponse &res_handshake);

bool validate_handshake_response(const FHttpRequest &req_handshake, FHttpResponse &res_handshake);



