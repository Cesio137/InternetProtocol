/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "net/common.h"

/**
 * 
 */

// HTTP
FString request_method_to_string(ERequestMethod method);

ERequestMethod string_to_request_method(const FString& method_str);

FString prepare_request(const FHttpRequest& req, const FString& address, const uint16_t port);

FString prepare_response(const FHttpResponse& res);

void res_append_header(FHttpResponse &res, const std::string &headerline);

void req_append_header(FHttpRequest &req, const std::string &headerline);