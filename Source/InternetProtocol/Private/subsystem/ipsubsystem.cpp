/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */

#include "subsystem/ipsubsystem.h"
#include "utils/utils.h"

UUDPServer* UInternetProtocolSubsystem::CreateUDPServer() {
	UUDPServer* server = NewObject<UUDPServer>();
	return server;
}

UUDPClient* UInternetProtocolSubsystem::CreateUDPClient() {
	UUDPClient* client = NewObject<UUDPClient>();
	return client;
}

UTCPServer* UInternetProtocolSubsystem::CreateTCPServer() {
	UTCPServer* server = NewObject<UTCPServer>();
	return server;
}

UTCPServerSsl* UInternetProtocolSubsystem::CreateTCPServerSsl(const FSecurityContextOpts &BindOpts) {
	UTCPServerSsl* server = NewObject<UTCPServerSsl>();
	server->InitializeSsl(BindOpts);
	return server;
}

UTCPClient* UInternetProtocolSubsystem::CreateTCPClient() {
	UTCPClient* client = NewObject<UTCPClient>();
	return client;
}

UTCPClientSsl* UInternetProtocolSubsystem::CreateTCPClientSsl(const FSecurityContextOpts& SecOpts) {
	UTCPClientSsl* client = NewObject<UTCPClientSsl>();
	client->Construct(SecOpts);
	return client;
}

UHttpClient* UInternetProtocolSubsystem::CreateHttpClient() {
	UHttpClient* client = NewObject<UHttpClient>();
	return client;
}
