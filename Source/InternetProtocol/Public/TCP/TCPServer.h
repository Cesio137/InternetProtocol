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
#include "Net/Commons.h"
#include "TCPServer.generated.h"

/**
 * 
 */

UCLASS(Blueprintable, BlueprintType, Category = "IP|TCP")
class INTERNETPROTOCOL_API UTCPServer : public UObject
{
	GENERATED_BODY()
public:
	virtual void BeginDestroy() override
	{
		if (TCP.acceptor.is_open()) Close();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Remote")
	void SetSocket(const EProtocolType Protocol = EProtocolType::V4, const int Port = 3000, const int MaxListenningConnection = 2147483647)
	{
		TcpProtocol = Protocol;
		TcpPort = Port;
		Backlog = MaxListenningConnection;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|TCP|Acceptor")
	FTCPAcceptor GetAcceptor()
	{
		return TCP.acceptor;
	}
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|TCP|Socket")
	const TArray<FTCPSocket> GetSockets()
	{
		TArray<FTCPSocket> sockets;
		for (const socket_ptr& socket_ref : TCP.sockets)
		{
			sockets.Add(socket_ref);
		}
		return sockets;
	}	

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Settings")
	void SetMaxSendBufferSize(int Value = 1400) { MaxSendBufferSize = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|TCP|Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Settings")
	void SetSplitPackage(bool Value = true) { bSplitBuffer = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|TCP|Settings")
	bool GetSplitPackage() const { return bSplitBuffer; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Message")
	bool SendStrTo(const FString& Message, const FTCPSocket& Socket);
	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Message")
	bool SendBufferTo(const TArray<uint8>& Buffer, const FTCPSocket& Socket);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Connection")
	bool Open();

	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Connection")
	void Close();

	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Connection")
	void DisconnectSocket(const FTCPSocket& Socket);

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|TCP|Error")
	FErrorCode GetErrorCode() const { return ErrorCode; }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateTcpAcceptor OnSocketAccepted;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateBytesTransferred OnBytesTransferred;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateSocketMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateTcpSocketMessage OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateTcpSocketError OnSocketDisconnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FAsioTcpServer TCP;
	asio::error_code ErrorCode;
	EProtocolType TcpProtocol = EProtocolType::V4;
	uint16_t TcpPort = 3000;
	int Backlog = 2147483647;
	bool bSplitBuffer = false;
	int MaxSendBufferSize = 1400;
	TMap<socket_ptr, TSharedPtr<asio::streambuf>> ListenerBuffer;

	void disconnect_socket_after_error(const asio::error_code& error, const socket_ptr& socket);
	void package_string(const FString& str, const socket_ptr &socket);
	void package_buffer(const TArray<uint8>& buffer, const socket_ptr &socket);
	void consume_response_buffer(const socket_ptr &socket);
	void run_context_thread();
	void accept(const asio::error_code& error, const socket_ptr& socket);
	void write(const asio::error_code& error, const size_t bytes_sent, const socket_ptr& socket);
	void read(const asio::error_code& error, const size_t bytes_recvd, const socket_ptr& socket);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|TCP")
class INTERNETPROTOCOL_API UTCPServerSsl : public UObject
{
	GENERATED_BODY()
public:
	virtual void BeginDestroy() override
	{
		if (TCP.acceptor.is_open()) Close();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Remote")
	void SetSocket(const EProtocolType Protocol = EProtocolType::V4, const int Port = 3000, const int MaxListenningConnection = 2147483647)
	{
		TcpProtocol = Protocol;
		TcpPort = Port;
		Backlog = MaxListenningConnection;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|TCP|Context")
	FTCPSslContext GetSslContext()
	{
		return TCP.ssl_context;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|TCP|Acceptor")
	FTCPAcceptor GetAcceptor()
	{
		return TCP.acceptor;
	}
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|TCP|Socket")
	const TArray<FTCPSslSocket> GetSslSockets()
	{
		TArray<FTCPSslSocket> sockets;
		for (const ssl_socket_ptr& socket_ref : TCP.ssl_sockets)
		{
			sockets.Add(*socket_ref);
		}
		return sockets;
	}	

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Settings")
	void SetMaxSendBufferSize(int Value = 1400) { MaxSendBufferSize = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|TCP|Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Settings")
	void SetSplitPackage(bool Value = true) { bSplitBuffer = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|TCP|Settings")
	bool GetSplitPackage() const { return bSplitBuffer; }

	/*SECURITY LAYER*/
	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Security Layer")
	bool LoadPrivateKeyData(const FString& KeyData) noexcept
	{
		if (KeyData.IsEmpty()) return false;
		std::string key = TCHAR_TO_UTF8(*KeyData);
		const asio::const_buffer buffer(key.data(), key.size());
		TCP.ssl_context.use_private_key(buffer, asio::ssl::context::pem, ErrorCode);
		if (ErrorCode)
		{
			ensureMsgf(!ErrorCode, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ErrorCode.value(),
					   ErrorCode.message().c_str());
			OnError.Broadcast(ErrorCode);
			return false;
		}
		return true;
	}

	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Security Layer")
	bool LoadPrivateKeyFile(const FString& FileName)
	{
		if (FileName.IsEmpty()) return false;
		std::string file = TCHAR_TO_UTF8(*FileName);
		TCP.ssl_context.use_private_key_file(file, asio::ssl::context::pem, ErrorCode);
		if (ErrorCode)
		{
			ensureMsgf(!ErrorCode, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ErrorCode.value(),
					   ErrorCode.message().c_str());
			OnError.Broadcast(ErrorCode);
			return false;
		}
		return true;
	}

	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Security Layer")
	bool LoadCertificateData(const FString& CertData)
	{
		if (CertData.IsEmpty()) return false;
		std::string cert = TCHAR_TO_UTF8(*CertData);
		const asio::const_buffer buffer(cert.data(), cert.size());
		TCP.ssl_context.use_certificate(buffer, asio::ssl::context::pem, ErrorCode);
		if (ErrorCode)
		{
			ensureMsgf(!ErrorCode, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ErrorCode.value(),
					   ErrorCode.message().c_str());
			OnError.Broadcast(ErrorCode);
			return false;
		}
		return true;
	}

	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Security Layer")
	bool LoadCertificateFile(const FString& FileName)
	{
		if (FileName.IsEmpty()) return false;
		asio::error_code ec;
		std::string file = TCHAR_TO_UTF8(*FileName);
		TCP.ssl_context.use_certificate_file(file, asio::ssl::context::pem, ErrorCode);
		if (ErrorCode)
		{
			ensureMsgf(!ErrorCode, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ErrorCode.value(),
					   ErrorCode.message().c_str());
			OnError.Broadcast(ErrorCode);
			return false;
		}
		return true;
	}

	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Security Layer")
	bool LoadCertificateChainData(const FString& CertChainData)
	{
		if (CertChainData.IsEmpty()) return false;
		std::string cert_chain = TCHAR_TO_UTF8(*CertChainData);
		const asio::const_buffer buffer(cert_chain.data(),
										cert_chain.size());
		TCP.ssl_context.use_certificate_chain(buffer, ErrorCode);
		if (ErrorCode)
		{
			ensureMsgf(!ErrorCode, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ErrorCode.value(),
					   ErrorCode.message().c_str());
			OnError.Broadcast(ErrorCode);
			return false;
		}
		return true;
	}

	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Security Layer")
	bool LoadCertificateChainFile(const FString& FileName)
	{
		if (FileName.IsEmpty()) return false;
		std::string file = TCHAR_TO_UTF8(*FileName);
		TCP.ssl_context.use_certificate_chain_file(file, ErrorCode);
		if (ErrorCode)
		{
			ensureMsgf(!ErrorCode, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ErrorCode.value(),
					   ErrorCode.message().c_str());
			OnError.Broadcast(ErrorCode);
			return false;
		}
		return true;
	}

	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Security Layer")
	bool LoadVerifyFile(const FString& FileName)
	{
		if (FileName.IsEmpty()) return false;
		std::string file = TCHAR_TO_UTF8(*FileName);
		TCP.ssl_context.load_verify_file(file, ErrorCode);
		if (ErrorCode)
		{
			ensureMsgf(!ErrorCode, TEXT("<ASIO ERROR>\nError code: %d\n%hs\n<ASIO ERROR/>"), ErrorCode.value(),
					   ErrorCode.message().c_str());
			OnError.Broadcast(ErrorCode);
			return false;
		}
		return true;
	}

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Message")
	bool SendStrTo(const FString& Message, const FTCPSslSocket& SslSocket);
	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Message")
	bool SendBufferTo(const TArray<uint8>& Buffer, const FTCPSslSocket& SslSocket);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Connection")
	bool Open();

	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Connection")
	void Close();

	UFUNCTION(BlueprintCallable, Category = "IP|TCP|Connection")
	void DisconnectSocket(const FTCPSslSocket& SslSocket);

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|TCP|Error")
	FErrorCode GetErrorCode() const { return ErrorCode; }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateTcpSslAcceptor OnSocketAccepted;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateBytesTransferred OnBytesTransferred;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateSslSocketMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateTcpSslSocketMessage OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateTcpSslSocketError OnSocketDisconnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateTcpSslSocketError  OnSocketError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||UDP||Events")
	FDelegateError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FAsioTcpServerSsl TCP;
	asio::error_code ErrorCode;
	EProtocolType TcpProtocol = EProtocolType::V4;
	uint16_t TcpPort = 3000;
	int Backlog = 2147483647;
	bool bSplitBuffer = true;
	int MaxSendBufferSize = 1400;
	TMap<ssl_socket_ptr, TSharedPtr<asio::streambuf>> ListenerBuffer;

	void disconnect_socket_after_error(const asio::error_code& error, const ssl_socket_ptr &ssl_socket);
	void package_string(const FString& str, const ssl_socket_ptr &ssl_socket);
	void package_buffer(const TArray<uint8>& buffer, const ssl_socket_ptr &ssl_socket);
	void consume_response_buffer(const ssl_socket_ptr &socket);
	void run_context_thread();
	void accept(const asio::error_code& error, ssl_socket_ptr &ssl_socket);
	void ssl_handshake(const asio::error_code &error, ssl_socket_ptr &ssl_socket);
	void write(const asio::error_code& error, const size_t bytes_sent, const ssl_socket_ptr &ssl_socket);
	void read(const asio::error_code& error, const size_t bytes_recvd, const ssl_socket_ptr &ssl_socket);
};
