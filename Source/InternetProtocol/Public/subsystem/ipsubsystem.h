/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "udp/udpserver.h"
#include "udp/udpclient.h"
#include "tcp/tcpclient.h"
#include "Subsystems/WorldSubsystem.h"

#include "ipsubsystem.generated.h"

/**
 * 
 */
UCLASS()
class INTERNETPROTOCOL_API UInternetProtocolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UUDPServer* CreateUDPServer();
	
	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UUDPClient* CreateUDPClient();

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UTCPClient* CreateTCPClient();
private:
	
};
