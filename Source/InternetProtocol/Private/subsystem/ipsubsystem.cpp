/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#include "subsystem/ipsubsystem.h"
#include "utils/utils.h"

void UInternetProtocolSubsystem::Deinitialize()
{
	UUtilsFunctionLibrary::StopThreads();
	Super::Deinitialize();
}

UUDPServer* UInternetProtocolSubsystem::CreateUDPServer() {
	UUDPServer* server = NewObject<UUDPServer>();
	return server;
}

UUDPClient* UInternetProtocolSubsystem::CreateUDPClient() {
	UUDPClient* client = NewObject<UUDPClient>();
	return client;
}

UTCPClient* UInternetProtocolSubsystem::CreateTCPClient() {
	UTCPClient* client = NewObject<UTCPClient>();
	return client;
}
