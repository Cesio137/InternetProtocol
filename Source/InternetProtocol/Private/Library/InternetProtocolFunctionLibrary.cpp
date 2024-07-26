// Fill out your copyright notice in the Description page of Project Settings.

#include "Library/InternetProtocolFunctionLibrary.h"

FString UInternetProtocolFunctionLibrary::toString(const TArray<uint8>& value)
{
	FUTF8ToTCHAR utf8_char(reinterpret_cast<const ANSICHAR*>(value.GetData()), value.Num());
	FString str(utf8_char.Length(), utf8_char.Get());
	return str;
}

TArray<FString> UInternetProtocolFunctionLibrary::DeserializeHeaderLine(const FString& value)
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
