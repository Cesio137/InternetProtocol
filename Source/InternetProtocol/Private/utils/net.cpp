/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#include "utils/net.h"

#include "utils/buffer.h"

FString request_method_to_string(ERequestMethod method) {
	switch (method) {
	case ERequestMethod::CONNECT: return "CONNECT";
	case ERequestMethod::DEL: return "DELETE";
	case ERequestMethod::GET: return "GET";
	case ERequestMethod::HEAD: return "HEAD";
	case ERequestMethod::OPTIONS: return "OPTIONS";
	case ERequestMethod::PATCH: return "PATCH";
	case ERequestMethod::POST: return "POST";
	case ERequestMethod::PUT: return "PUT";
	case ERequestMethod::TRACE: return "TRACE";
	default:
		return "";
	}
}

ERequestMethod string_to_request_method(const FString& method_str) {
	static const TMap<FString, ERequestMethod> method_map = {
		{"CONNECT", ERequestMethod::CONNECT},
		{"DELETE", ERequestMethod::DEL},
		{"GET", ERequestMethod::GET},
		{"HEAD", ERequestMethod::HEAD},
		{"OPTIONS", ERequestMethod::OPTIONS},
		{"PATCH", ERequestMethod::PATCH},
		{"POST", ERequestMethod::POST},
		{"PUT", ERequestMethod::PUT},
		{"TRACE", ERequestMethod::TRACE}
	};

	if (method_map.Contains(method_str)) {
		return method_map[method_str];
	}

	return ERequestMethod::UNKNOWN;
}

FString prepare_request(const FHttpRequest& req, const FString& address, const uint16_t port) {
	
	FString payload;
	payload.Reserve(8192);

	payload = request_method_to_string(req.Method) + " " + req.Path;
	if (req.Params.Num())
	{
		payload += "?";
		bool first = true;
		for (const TPair<FString, FString> &param : req.Params)
		{
			if (!first) payload += "&";
			payload += param.Key + "=" + param.Value;
			first = false;
		}
	}
	payload += " HTTP/" + req.Version + "\r\n";

	payload += "Host: " + address;
	if (port != 80 && port != 443) payload += ":" + FString::FromInt(port);
	payload += "\r\n";

	if (req.Headers.Num())
	{
		for (const TPair<FString, FString> &header : req.Headers) {
			payload += header.Key + ": " + header.Value + "\r\n";
		}
		if (req.Headers.Contains("Content-Length") && req.Body.Len() > 0) {
			payload +=
				"Content-Length: " + FString::FromInt(req.Body.Len()) + "\r\n";
		}
		else if (req.Headers.Contains("content-length") && req.Body.Len() > 0) {
			payload +=
				"content-length: " + FString::FromInt(req.Body.Len()) + "\r\n";
		}
	}
	payload += "\r\n";

	if (!req.Body.IsEmpty()) payload += req.Body;

	if (payload.GetAllocatedSize() > payload.Len())
		payload.Shrink();

	return payload;
}

FString prepare_response(const FHttpResponse& res) {
	FString payload;
	payload.Reserve(8192);

	payload = "HTTP/" + res.Version + " " + FString::FromInt(res.Status_Code) + " " + FString::FromInt(res.Status_Code)
			  + "\r\n";
	if (res.Headers.Num()) {
		for (const TPair<FString, FString> &header: res.Headers) {
			payload += header.Key + ": " + header.Value + "\r\n";
		}
		if (res.Headers.Contains("Content-Length") && res.Body.Len() > 0) {
			payload +=
				"Content-Length: " + FString::FromInt(res.Body.Len()) + "\r\n";
		}
		else if (res.Headers.Contains("content-length") && res.Body.Len() > 0) {
			payload +=
				"content-length: " + FString::FromInt(res.Body.Len()) + "\r\n";
		}
	}
	payload += "\r\n";
	if (!res.Body.IsEmpty()) payload += res.Body;

	if (payload.GetAllocatedSize() > payload.Len())
		payload.Shrink();

	return payload;
}

void res_append_header(FHttpResponse& res, const std::string& headerline) {
	size_t pos = headerline.find(':');
	if (pos != std::string::npos) {
		std::string key = trim_whitespace(headerline.substr(0, pos));
		string_to_lower(key);
		std::string value = headerline.substr(pos + 1);
		/*
		std::vector<std::string> values = split_string(value, ';');
		std::transform(
			values.begin(), values.end(), values.begin(),
			[&](const std::string &str) { return trim_whitespace(str); });
		*/
		res.Headers.Add(key.c_str(), value.c_str());
	}
}

void req_append_header(FHttpRequest& req, const std::string& headerline) {
	size_t pos = headerline.find(':');
	if (pos != std::string::npos) {
		std::string key = trim_whitespace(headerline.substr(0, pos));
		string_to_lower(key);
		std::string value = headerline.substr(pos + 1);
		/*
		std::vector<std::string> values = split_string(value, ';');
		std::transform(
			values.begin(), values.end(), values.begin(),
			[&](const std::string &str) { return trim_whitespace(str); });
		*/
		req.Headers.Add(key.c_str(), value.c_str());
	}
}
