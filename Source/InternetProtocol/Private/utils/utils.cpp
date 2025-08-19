/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#include "utils/utils.h"
#include "net/common.h"
#include "utils/buffer.h"

void UUtilsFunctionLibrary::JoinThreads() {
	thread_pool().join();
}

void UUtilsFunctionLibrary::StopThreads() {
	thread_pool().stop();
}

FString UUtilsFunctionLibrary::BufferToString(const TArray<uint8>& Value) {
	FUTF8ToTCHAR utf8_char(reinterpret_cast<const ANSICHAR*>(Value.GetData()), Value.Num());
	FString str(utf8_char.Length(), utf8_char.Get());
	return str;
}

TArray<FString> UUtilsFunctionLibrary::SplitString(const FString& Str, const FString& Delimiter) {
	TArray<FString> Tokens;
    
	if (Str.IsEmpty()) {
		return Tokens;
	}
    
	int32 StartIndex = 0;
	int32 CurrentIndex = 0;
	std::string str = TCHAR_TO_UTF8(*Str);
	trim_whitespace(str);
	FString trimmed_str(str.c_str());
    
	while (CurrentIndex < Str.Len()) {
		if (trimmed_str[CurrentIndex] == Delimiter[0]) {
			FString Token = trimmed_str.Mid(StartIndex, CurrentIndex - StartIndex);
			Tokens.Add(Token);
			StartIndex = CurrentIndex + 1;
		}
		CurrentIndex++;
	}
    
	if (StartIndex <= Str.Len()) {
		FString LastToken = trimmed_str.Mid(StartIndex);
		Tokens.Add(LastToken);
	}
    
	return Tokens;

}
