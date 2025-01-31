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
#include "HttpServer.generated.h"

/**
 * 
 */

UCLASS(NotBlueprintable, BlueprintType, Category = "IP|HTTP")
class INTERNETPROTOCOL_API UHttpServer : public UObject
{
	GENERATED_BODY()

public:
	UHttpServer()
	{
		Headers.Add("User-Agent", "ASIO");
		Headers.Add("Connection", "close");
	}

	virtual void BeginDestroy() override
	{
		if (TCP.acceptor.is_open())
		{
			Close();
		}
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Remote")
	void SetSocket(const EProtocolType Protocol = EProtocolType::V4, const int Port = 3000,
	               const int MaxListenningConnection = 2147483647)
	{
		TcpProtocol = Protocol;
		TcpPort = Port;
		Backlog = MaxListenningConnection;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Acceptor")
	FTCPAcceptor GetAcceptor()
	{
		return TCP.acceptor;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Socket")
	const TArray<FTCPSocket> GetSockets()
	{
		TArray<FTCPSocket> sockets;
		for (const socket_ptr& socket_ref : TCP.sockets)
		{
			sockets.Add(socket_ref);
		}
		return sockets;
	}

	/*REQUEST DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void AppendHeaders(const FString& Key, const FString& value) { Headers.Add(Key, value); }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void ClearHeaders() { Headers.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void RemoveHeader(const FString& Key) { Headers.Remove(Key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	bool HasHeader(const FString& Key) const { return Headers.Contains(Key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	TMap<FString, FString> GetHeaders() const { return Headers; }

	/*RESPONSE*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Response")
	bool SendResponse(const FServerResponse& Response, const FTCPSocket& Socket);

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Response")
	bool SendErrorResponse(const int StatusCode, const FServerResponse& Response, const FTCPSocket& Socket);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Connection")
	bool Open();

	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Connection")
	void Close();

	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Connection")
	void DisconnectSocket(const FTCPSocket& Socket);

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Error")
	FErrorCode GetErrorCode() const { return ErrorCode; }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateTcpAcceptor OnSocketAccepted;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateBytesTransferred OnBytesTransferred;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpServerRequest OnRequestReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpDataError OnRequestError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateSocketMessageSent OnResponseSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateTcpSocketError OnSocketDisconnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FAsioTcpServer TCP;
	asio::error_code ErrorCode;
	EProtocolType TcpProtocol = EProtocolType::V4;
	uint16_t TcpPort = 3000;
	int Backlog = 2147483647;
	TMap<FString, FString> Headers;
	TMap<socket_ptr, TSharedPtr<asio::streambuf>> RequestBuffers;

	void disconnect_socket_after_error(const asio::error_code& error, const socket_ptr& socket);
	void process_response(const FServerResponse& response, const socket_ptr& socket);
	void process_error_response(const int status_code, const FServerResponse& response, const socket_ptr& socket);
	void consume_request_buffer(const socket_ptr& socket);
	void run_context_thread();
	void accept(const std::error_code& error, socket_ptr& socket);
	void write_response(const asio::error_code& error, const size_t bytes_sent, const socket_ptr& socket,
	                    const bool close_connection);
	void read_status_line(const asio::error_code& error, const size_t bytes_recvd, const socket_ptr& socket);
	void read_headers(const asio::error_code& error, const size_t bytes_recvd, FServerRequest& request,
	                  const socket_ptr& socket);
	void read_body(FServerRequest& request, const socket_ptr& socket);
};

UCLASS(NotBlueprintable, BlueprintType, Category = "IP|HTTP")
class INTERNETPROTOCOL_API UHttpServerSsl : public UObject
{
	GENERATED_BODY()

public:
	UHttpServerSsl()
	{
		Headers.Add("User-Agent", "ASIO");
		Headers.Add("Connection", "close");
	}

	virtual void BeginDestroy() override
	{
		if (TCP.acceptor.is_open())
		{
			Close();
		}
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Remote")
	void SetSocket(const EProtocolType Protocol = EProtocolType::V4, const int Port = 3000,
	               const int max_listen_conn = 2147483647)
	{
		TcpProtocol = Protocol;
		TcpPort = Port;
		Backlog = max_listen_conn;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Context")
	FTCPSslContext GetSslContext()
	{
		return TCP.ssl_context;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Acceptor")
	FTCPAcceptor GetAcceptor()
	{
		return TCP.acceptor;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Socket")
	const TArray<FTCPSslSocket> GetSslSockets()
	{
		TArray<FTCPSslSocket> sockets;
		for (const ssl_socket_ptr& socket_ref : TCP.ssl_sockets)
		{
			sockets.Add(*socket_ref);
		}
		return sockets;
	}

	/*REQUEST DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void AppendHeaders(const FString& Key, const FString& Value) { Headers.Add(Key, Value); }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void ClearHeaders() { Headers.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void RemoveHeader(const FString& Key) { Headers.Remove(Key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	bool HasHeader(const FString& Key) const { return Headers.Contains(Key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	TMap<FString, FString> GetHeaders() const { return Headers; }

	/*SECURITY LAYER*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
	bool LoadPrivateKeyData(const FString& KeyData) noexcept
	{
		if (KeyData.IsEmpty())
		{
			return false;
		}
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

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
	bool LoadPrivateKeyFile(const FString& FileName)
	{
		if (FileName.IsEmpty())
		{
			return false;
		}
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

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
	bool LoadCertificateData(const FString& CertData)
	{
		if (CertData.IsEmpty())
		{
			return false;
		}
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

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
	bool LoadCertificateFile(const FString& FileName)
	{
		if (FileName.IsEmpty())
		{
			return false;
		}
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

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
	bool LoadCertificateChainData(const FString& CertChainData)
	{
		if (CertChainData.IsEmpty())
		{
			return false;
		}
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

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
	bool LoadCertificateChainFile(const FString& FileName)
	{
		if (FileName.IsEmpty())
		{
			return false;
		}
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

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
	bool LoadVerifyFile(const FString& FileName)
	{
		if (FileName.IsEmpty())
		{
			return false;
		}
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

	/*RESPONSE*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Response")
	bool SendResponse(const FServerResponse& Response, const FTCPSslSocket& SslSocket);

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Response")
	bool SendErrorResponse(const int StatusCode, const FServerResponse& Response, const FTCPSslSocket& SslSocket);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Connection")
	bool Open();

	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Connection")
	void Close();

	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Connection")
	void DisconnectSocket(const FTCPSslSocket& SslSocket);

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Error")
	FErrorCode GetErrorCode() const { return ErrorCode; }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateTcpSslAcceptor OnSocketAccepted;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateBytesTransferred OnBytesTransferred;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpSslServerRequest OnRequestReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpDataError OnRequestError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateSslSocketMessageSent OnResponseSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateTcpSslSocketError OnSocketDisconnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FAsioTcpServerSsl TCP;
	asio::error_code ErrorCode;
	EProtocolType TcpProtocol = EProtocolType::V4;
	uint16_t TcpPort = 3000;
	int Backlog = 2147483647;
	TMap<FString, FString> Headers;
	TMap<ssl_socket_ptr, TSharedPtr<asio::streambuf>> RequestBuffers;

	void disconnect_socket_after_error(const asio::error_code& error, const ssl_socket_ptr& ssl_socket);
	void process_response(FServerResponse& response, const ssl_socket_ptr& ssl_socket);
	void process_error_response(const int status_code, const FServerResponse& response,
	                            const ssl_socket_ptr& ssl_socket);
	void consume_request_buffer(const ssl_socket_ptr& ssl_socket);
	void run_context_thread();
	void accept(const std::error_code& error, ssl_socket_ptr& ssl_socket);
	void ssl_handshake(const asio::error_code& error, ssl_socket_ptr& ssl_socket);
	void write_response(const asio::error_code& error, const size_t bytes_sent, const ssl_socket_ptr& ssl_socket,
	                    const bool close_connection);
	void read_status_line(const asio::error_code& error, const size_t bytes_recvd, const ssl_socket_ptr& ssl_socket);
	void read_headers(const asio::error_code& error, const size_t bytes_recvd, FServerRequest& request,
	                  const ssl_socket_ptr& ssl_socket);
	void read_body(FServerRequest& request, const ssl_socket_ptr& ssl_socket);
};
