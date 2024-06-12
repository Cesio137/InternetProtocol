// Fill out your copyright notice in the Description page of Project Settings.


#include "Library/InternetProtocolFunctionLibrary.h"

TArray<FString> UInternetProtocolFunctionLibrary::DeserializeHeaderLine(const FString& value)
{
	TArray<FString> values;
	value.ParseIntoArray(values, TEXT(";"), true);
	for (FString& str : values) {
		FString Result = str;
		Result.TrimStartAndEndInline();
		str = Result;
	}
	
	return values;
}
