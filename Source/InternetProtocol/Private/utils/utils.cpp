/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#include "utils/utils.h"
#include "net/common.h"

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
