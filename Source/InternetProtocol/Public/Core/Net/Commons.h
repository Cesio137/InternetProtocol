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

USTRUCT(Blueprintable, Category = "IP")
struct FRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	TMap<FString, FString> params;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	EVerb verb = EVerb::GET;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString path = "/";
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString version = "2.0";
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	TMap<FString, FString> headers;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString body;

	void clear()
	{
		params.Empty();
		verb = EVerb::GET;
		path = "/";
		version = "2.0";
		headers.Empty();
		body.Empty();
	}
};

USTRUCT(Blueprintable, Category = "IP")
struct FResponse
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	TMap<FString, FString> Headers;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	FString Content;
	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	int ContentLength;

	void appendHeader(const FString& headerline) {
		int32 Pos;
		if (headerline.FindChar(TEXT(':'), Pos)) {
			FString key = trimWhitespace(headerline.Left(Pos));
			FString value = trimWhitespace(headerline.Mid(Pos + 1));
			if (key == "Content-Length")
			{
				ContentLength = FCString::Atoi(*value);
				return;
			}
			/*
			TArray<FString> values;
			value.ParseIntoArray(values, TEXT(";"), true);
			for (FString& str : values) {
				str = trimWhitespace(str);
			}
			*/
			Headers.Add(key, value);
		}
	}

	void setContent(const FString& value) {
		if (value.IsEmpty())
			return;
		//ContentLenght = value.Len();
		Content = value;
	}

	void appendContent(const FString& value) {
		//ContentLenght += value.Len();
		Content.Append(value);
	}

	void clear() {
		Headers.Empty();
		ContentLength = 0;
		Content.Empty();
	}

private:
	FString trimWhitespace(const FString& str) const {
		FString Result = str;
		Result.TrimStartAndEndInline();
		return Result;
	}
};

/*
UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UHeader : public UObject
{
	GENERATED_BODY()
public:
    UHeader() {}
    ~UHeader() {
        clear();
    }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
    void appendHeader(const FString& headerline) {
    	int32 Pos;
    	if (headerline.FindChar(TEXT(':'), Pos)) {
    		FString key = trimWhitespace(headerline.Left(Pos));
    		FString value = trimWhitespace(headerline.Mid(Pos + 1));
    		TArray<FString> values;
    		value.ParseIntoArray(values, TEXT(";"), true);
    		for (FString& str : values) {
    			str = trimWhitespace(str);
    		}
    		headers.Add(key, values);
    	}
    }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
    void setContent(const FString& value) {
        if (value.IsEmpty())
            return;
        contentLenght = value.Len();
        content = value;
    }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
    void appendContent(const FString& value) {
        contentLenght += value.Len();
        content.Append(value);
    }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
	TArray<FString> getKeys() const
    {
    	TArray<FString> keys;
    	headers.GetKeys(keys);
    	return keys;
    }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
    TArray<FString> getHeader(const FString& key) const
	{
    	TArray<FString> result;
	    if (headers.Contains(key))
			result = headers[key];
        return result;
    }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
	bool hasHeader(const FString& key) const { return headers.Contains(key); }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
    int getContentLenght() const {
        return contentLenght;
    }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
    FString getContent() const {
        return content;
    }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
    void clear() {
        headers.Empty();
        contentLenght = 0;
        content.Empty();
    }

private:
    TMap<FString, TArray<FString>> headers;
    int contentLenght = 0;
    FString content;

	FString trimWhitespace(const FString& str) const {
    	FString Result = str;
    	Result.TrimStartAndEndInline();
    	return Result;
    }

};
*/
