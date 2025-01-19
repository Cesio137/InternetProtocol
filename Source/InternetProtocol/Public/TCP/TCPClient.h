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
#include "UObject/NoExportTypes.h"
#include "Net/Commons.h"
#include "Net/Message.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "TCPClient.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateTcpConnection);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpTransferred, const int, BytesSent, const int, BytesRecvd);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateTcpMessageSent);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpMessageReceived, const FTcpMessage, message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpError, FErrorCode, ErrorCode);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UTCPClient : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override
	{
		TCP.resolver.cancel();
		if (TCP.socket.is_open()) Close();
		consume_response_buffer();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Remote")
	void SetHost(const FString& ip = "localhost", const FString& port = "3000")
	{
		Host = ip;
		Service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Socket")
	FTCPSocket GetSocket() { return TCP.socket; }	

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void SetMaxSendBufferSize(int value = 1400) { MaxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void SetSplitPackage(bool value = true) { bSplitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	bool GetSplitPackage() const { return bSplitBuffer; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool SendStr(const FString& message);
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool SendBuffer(const TArray<uint8>& buffer);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	bool Connect();

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	void Close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Error")
	FErrorCode GetErrorCode() const { return ErrorCode; }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpConnection OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpTransferred OnBytesTransferred;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpError OnSocketError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FAsioTcpClient TCP;
	asio::error_code ErrorCode;
	FString Host = "localhost";
	FString Service = "3000";
	bool bSplitBuffer = true;
	int MaxSendBufferSize = 1400;
	asio::streambuf ResponseBuffer;

	void package_string(const FString& str);
	void package_buffer(const TArray<uint8>& buffer);
	void consume_response_buffer();
	void run_context_thread();
	void resolve(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void conn(const asio::error_code& error);
	void write(const asio::error_code& error, const size_t bytes_sent);
	void read(const asio::error_code& error, const size_t bytes_recvd);
};

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UTCPClientSsl : public UObject
{
	GENERATED_BODY()

public:	
	virtual void BeginDestroy() override
	{
		TCP.resolver.cancel();
		if (TCP.ssl_socket.lowest_layer().is_open()) Close();
		consume_response_buffer();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Remote")
	void SetHost(const FString& ip = "localhost", const FString& port = "3000")
	{
		Host = ip;
		Service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Context")
	FSslContext GetContext() { return TCP.ssl_context; }
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Socket")
	FTCPSslSocket GetSocket() { return TCP.ssl_socket; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Socket")
	void UpdateSslSocket() { TCP.ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(TCP.context, TCP.ssl_context); }
	
	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void SetMaxSendBufferSize(int value = 1400) { MaxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void SetSplitPackage(bool value = true) { SplitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	bool GetSplitPackage() const { return SplitBuffer; }

	/*SECURITY LAYER*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool LoadPrivateKeyData(const FString& key_data) noexcept
	{
		if (key_data.IsEmpty()) return false;
		std::string key = TCHAR_TO_UTF8(*key_data);
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool LoadPrivateKeyFile(const FString& filename)
	{
		if (filename.IsEmpty()) return false;
		std::string file = TCHAR_TO_UTF8(*filename);
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool LoadCertificateData(const FString& cert_data)
	{
		if (cert_data.IsEmpty()) return false;
		std::string cert = TCHAR_TO_UTF8(*cert_data);
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool LoadCertificateFile(const FString& filename)
	{
		if (filename.IsEmpty()) return false;
		asio::error_code ec;
		std::string file = TCHAR_TO_UTF8(*filename);
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool LoadCertificateChainData(const FString& cert_chain_data)
	{
		if (cert_chain_data.IsEmpty()) return false;
		std::string cert_chain = TCHAR_TO_UTF8(*cert_chain_data);
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool LoadCertificateChainFile(const FString& filename)
	{
		if (filename.IsEmpty()) return false;
		std::string file = TCHAR_TO_UTF8(*filename);
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool LoadVerifyFile(const FString& filename)
	{
		if (filename.IsEmpty()) return false;
		std::string file = TCHAR_TO_UTF8(*filename);
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
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool SendStr(const FString& message);
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool SendBuffer(const TArray<uint8>& buffer);

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	bool Connect();

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	void Close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Error")
	FErrorCode GetErrorCode() const { return ErrorCode; }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpConnection OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpTransferred OnBytesTransferred;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpError OnSocketError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FAsioTcpSslClient TCP;
	asio::error_code ErrorCode;
	FString Host = "localhost";
	FString Service = "3000";
	bool SplitBuffer = true;
	int MaxSendBufferSize = 1400;
	asio::streambuf ResponseBuffer;

	void package_string(const FString& str);
	void package_buffer(const TArray<uint8>& buffer);
	void consume_response_buffer();
	void run_context_thread();
	void resolve(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void conn(const asio::error_code& error);
	void ssl_handshake(const asio::error_code& error);
	void write(const asio::error_code& error, const size_t bytes_sent);
	void read(const asio::error_code& error, const size_t bytes_recvd);
};
