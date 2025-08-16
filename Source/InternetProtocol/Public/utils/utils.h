/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "utils.generated.h"

/**
 * 
 */
UCLASS()
class INTERNETPROTOCOL_API UUtilsFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "IP|Utils|Threads")
	static void JoinThreads();

	UFUNCTION(BlueprintCallable, Category = "IP|Utils|Threads")
	static void StopThreads();

	/*Binary functions*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="IP|Utils|Buffer")
	static FString BufferToString(const TArray<uint8>& value);
};
