#pragma once

#include "CoreMinimal.h"
#include "Library/InternetProtocolEnumLibrary.h"
THIRD_PARTY_INCLUDES_START
#include "asio.hpp"
THIRD_PARTY_INCLUDES_END
#include "InternetProtocolStructLibrary.generated.h"

/*HTTP*/
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
			Headers.Add(key, value);
		}
	}

	void setContent(const FString& value) {
		if (value.IsEmpty())
			return;
		Content = value;
	}

	void appendContent(const FString& value) {
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
