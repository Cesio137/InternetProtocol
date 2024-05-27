// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Library/StructLibrary.h"
#include "RequestObject.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API URequestObject : public UObject
{
	GENERATED_BODY()
public:
	URequestObject(){}
	~URequestObject(){}

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void setHost(const FString& host = "localhost", const FString& service = "")
	{
		_host = host;
		_service = service;
	}
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	FString getHost() const { return _host; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	FString getPort() const { return _service; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void setRequest(EHttpVerb request_method = EHttpVerb::GET, const FString& version = "2.0")
	{
		_verb = request_method;
		_version = version;
	}
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	EHttpVerb getRequestMethod() const { return _verb; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	FString getVersion() const { return _version; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void setPath(const FString& path = "/")
	{
		_path = path.IsEmpty() ? "/" : path;
	}
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	FString getPath() const { return _path; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void appendParams(const FString& key, const FString& value) { _params[key] = value; }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void clearParams() { _params.Empty(); }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void removeParam(const FString& key) { _params.Remove(key); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	bool hasParam(const FString& key) const { return _params.Contains(key); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	TMap<FString, FString> getParams() const { return _params; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void AppendHeaders(const FString& key, const FString& value) { _headers[key] = value; }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void ClearHeaders() { _headers.Empty(); }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void RemoveHeader(const FString& key) { _headers.Remove(key); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	bool hasHeader(const FString& key) const { return _headers.Contains(key); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	TMap<FString, FString> GetHeaders() const { return _headers; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void SetBody(const FString& value) { _body = value; }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void ClearBody() { _body.Empty(); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	FString GetBody() const { return _body; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	FString data();

private:
	FString _host;
	FString _service;
    TMap<FString, FString> _params;
    EHttpVerb _verb = EHttpVerb::GET;
    FString _path = "/";
    FString _version = "2.0";
    TMap<FString, FString> _headers;
    FString _body;

	const TMap<EHttpVerb, FString> _request_method = {
		{EHttpVerb::GET     , "GET"},
		{EHttpVerb::POST    , "POST"},
		{EHttpVerb::PUT     , "PUT"},
		{EHttpVerb::PATCH   , "PATCH"},
		{EHttpVerb::DEL     , "DELETE"},
		{EHttpVerb::COPY    , "COPY"},
		{EHttpVerb::HEAD    , "HEAD"},
		{EHttpVerb::OPTIONS , "OPTIONS"},
		{EHttpVerb::LOCK    , "LOCK"},
		{EHttpVerb::UNLOCK  , "UNLOCK"},
		{EHttpVerb::PROPFIND, "PROPFIND"},
	};

	
};
