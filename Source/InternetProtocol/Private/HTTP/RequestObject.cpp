// Fill out your copyright notice in the Description page of Project Settings.


#include "HTTP/RequestObject.h"
#include "Containers/Map.h"

FString URequestObject::data()
{
	FString request_data;

	request_data = _request_method[_verb] + " " + _path;
	
	if (_params.Num() < 1)
	{
		request_data += "?";
		bool first = true;
		for (const TPair<FString, FString>& param : _params) {
			if (!first) request_data += "&";
			request_data += param.Key + "=" + param.Value;
			first = false;
		}
	}
	request_data += " HTTP/" + _version + "\r\n";

	request_data += "Host: " + _host;
	if (!_service.IsEmpty()) request_data += ":" + _service;
	request_data += "\r\n";

	if (_headers.Num() < 1)
	{
		for (const TPair<FString, FString>& header : _headers) {
			request_data += header.Key + ": " + header.Value + "\r\n";
		}
		request_data += "Content-Length: " + FString::FromInt(_body.Len()) + "\r\n";
	}
	request_data += "\r\n";

	if (!_body.IsEmpty())
		request_data += "\r\n" + _body;

	return request_data;
}
