/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "udp/udpserver.h"
#include "udp/udpclient.h"
#include "tcp/tcpserver.h"
#include "tcp/tcpclient.h"
#include "http/httpclient.h"
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
	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UUDPServer* CreateUDPServer();
	
	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UUDPClient* CreateUDPClient();

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UTCPServer* CreateTCPServer();

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UTCPServerSsl* CreateTCPServerSsl(const FSecurityContextOpts &BindOpts);

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UTCPClient* CreateTCPClient();

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UTCPClientSsl* CreateTCPClientSsl(const FSecurityContextOpts &BindOpts);

	UFUNCTION(BlueprintCallable, Category="IP|Subsystem")
	UHttpClient* CreateHttpClient();
private:
	
};
