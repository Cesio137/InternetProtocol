/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "net/common.h"
#include "utils.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_DELEGATE(FDelegateAsync);


UCLASS()
class INTERNETPROTOCOL_API UUtilsFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "IP|Utils|Threads")
	static void JoinThreads();

	UFUNCTION(BlueprintCallable, Category = "IP|Utils|Threads")
	static void StopThreads();

	/*Buffer functions*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="IP|Utils|Buffer")
	static FString BufferToString(const TArray<uint8>& value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="IP|Utils|Buffer")
	static TArray<FString> SplitString(const FString& Str, const FString& Delimiter);
};
