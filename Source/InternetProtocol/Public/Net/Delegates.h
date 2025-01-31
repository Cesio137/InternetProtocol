/*
 * Copyright (c) 2023-2025 Nathan Miguel
 *
 * InternetProtocol is free library: you can redistribute it and/or modify it under the terms
 * of the GNU Affero General Public License as published by the Free Software Foundation,
 * version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
*/

#pragma once

#include "CoreMinimal.h"
#include "Net/Message.h"
#include "Library/InternetProtocolStructLibrary.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "Delegates.generated.h"

/*CONNECTION*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateConnection);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpAcceptor, const FTCPSocket, Socket);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpSslAcceptor, const FTCPSslSocket, Socket);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateBytesTransferred, const int, BytesSent, const int, BytesRecvd);

/*MESSAGE*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateMessageSent, const FErrorCode, HadError);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateSocketMessageSent, const FErrorCode, HadError, const FTCPSocket,
                                             Socket);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateSslSocketMessageSent, const FErrorCode, HadError,
                                             const FTCPSslSocket, SslSocket);

/*UDP*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateUdpMessage, const FUdpMessage, Message, const FUDPEndpoint,
                                             Endpoint);

/*TCP*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpMessage, const FTcpMessage&, Message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpSocketMessage, const FTcpMessage&, Message, const FTCPSocket&,
                                             Socket);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpSslSocketMessage, const FTcpMessage&, Message,
                                             const FTCPSslSocket&, Socket);

/*HTTP*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateHttpDataError, const FClientResponse, Response);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateHttpServerRequest, const FServerRequest, Request,
                                               FServerResponse, Response, const FTCPSocket, Socket);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateHttpSslServerRequest, FServerRequest, Request, FServerResponse,
                                               Response, const FTCPSslSocket, SslSocket);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateHttpClientResponse, const FClientResponse, Response);

/*WEBSOCKET*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateWsSocketHandshake, const FServerRequest, Request,
                                               FServerResponse, Response, const FTCPSocket, Socket);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateWsSslSocketHandshake, const FServerRequest, Request,
                                               FServerResponse, Response, const FTCPSslSocket, SslSocket);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateWsMessage, const FWsMessage, Message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateWsSocketMessage, const FWsMessage, Message, const FTCPSocket,
                                             Socket);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateWsSslSocketMessage, const FWsMessage, Message,
                                             const FTCPSslSocket, Socket);

/*ERROR*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateError, const FErrorCode, ErrorCode);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpSocketError, const FErrorCode, HadError, const FTCPSocket,
                                             Socket);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpSslSocketError, const FErrorCode, HadError,
                                             const FTCPSslSocket, SslSocket);

// FIX FOR "Unrecognized type" ERROR
UCLASS()
class UDelegates : public UObject
{
	GENERATED_BODY()

public:
};
