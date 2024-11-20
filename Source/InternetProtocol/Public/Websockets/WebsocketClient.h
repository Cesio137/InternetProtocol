/*
 * Copyright (c) 2023-2024 Nathan Miguel
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
#include "WebsocketClient.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateWsConnection);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateWsConnectionRetry, const int, attemps);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateWsMessageSent, const int, bytesSent);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateWsMessageReceived, const FWsMessage, message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateWsError, const int, code, const FString&, exception);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UWebsocketClient : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override
	{
		ShouldStopContext = true;
		TCP.resolver.cancel();
		if (IsConnected()) Close();
		ThreadPool->wait();
		consume_response_buffer();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Remote")
	void SetHost(const FString& url = "localhost", const FString& port = "")
	{
		Host = url;
		Service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Local")
	FString GetLocalAdress() const
	{
		if (IsConnected())
		{
			return TCP.socket.local_endpoint().address().to_string().c_str();
		}
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Local")
	FString GetLocalPort() const
	{
		if (IsConnected())
		{
			return FString::FromInt(TCP.socket.local_endpoint().port());
		}
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Remote")
	FString GetRemoteAdress() const
	{
		if (IsConnected())
		{
			return TCP.socket.remote_endpoint().address().to_string().c_str();
		}
		return Host;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Remote")
	FString GetRemotePort() const
	{
		if (IsConnected())
		{
			return FString::FromInt(TCP.socket.remote_endpoint().port());
		}
		return Service;
	}

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Settings")
	void SetTimeout(uint8 value = 3) { Timeout = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Settings")
	uint8 GetTimeout() const { return Timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Settings")
	void SetMaxAttemp(uint8 value = 3) { MaxAttemp = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Settings")
	uint8 GetMaxAttemp() const { return Timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Settings")
	void SetMaxSendBufferSize(int value = 1400) { MaxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Settings")
	void SetSplitPackage(bool value = true) { SplitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Settings")
	bool GetSplitPackage() const { return SplitBuffer; }


	/*HANDSHAKE*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void SetPath(const FString& value = "chat") { Handshake.path = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString GetPath() const { return Handshake.path; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void SetVersion(const FString& value = "1.1") { Handshake.version = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString GetVersion() const { return Handshake.version; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void SetKey(const FString& value = "dGhlIHNhbXBsZSBub25jZQ==") { Handshake.Sec_WebSocket_Key = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString GetKey() const { return Handshake.Sec_WebSocket_Key; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void SetOrigin(const FString& value = "client") { Handshake.origin = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString GetOrigin() const { return Handshake.origin; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void SetSetProtocol(const FString& value = "chat, superchat") { Handshake.Sec_WebSocket_Protocol = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString GetSecProtocol() const { return Handshake.Sec_WebSocket_Protocol; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void SetSetVersion(const FString& value = "13") { Handshake.Sec_Websocket_Version = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString GetSecVersion() const { return Handshake.Sec_Websocket_Version; }

	/*DATAFRAME*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Dataframe")
	void SetRSV1(bool value = false) { SDataFrame.rsv1 = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Dataframe")
	bool UseRSV1() const { return SDataFrame.rsv1; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Dataframe")
	void SetRSV2(bool value = false) { SDataFrame.rsv2 = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Dataframe")
	bool UseRSV2() const { return SDataFrame.rsv2; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Dataframe")
	void SetRSV3(bool value = false) { SDataFrame.rsv3 = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Dataframe")
	bool UseRSV3() const { return SDataFrame.rsv3; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Dataframe")
	void SetMask(bool value = true) { SDataFrame.mask = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Dataframe")
	bool UseMask() const { return SDataFrame.mask; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Message")
	bool Send(const FString& message);
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Message")
	bool SendRaw(const TArray<uint8> buffer);
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Message")
	bool SendPing();
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Message")
	bool AsyncRead();

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Connection")
	bool Connect();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Connection")
	bool IsConnected() const { return TCP.socket.is_open(); }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Connection")
	void Close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Error")
	int GetErrorCode() const { return TCP.error_code.value(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Error")
	FString GetErrorMessage() const { return TCP.error_code.message().c_str(); }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsConnection OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsConnectionRetry OnConnectionWillRetry;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsConnection OnPongReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsConnection OnCloseNotify;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsError OnError;

private:
	TUniquePtr<asio::thread_pool> ThreadPool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	bool ShouldStopContext = false;
	FString Host = "localhost";
	FString Service;
	uint8 Timeout = 3;
	uint8 MaxAttemp = 3;
	bool SplitBuffer = false;
	int MaxSendBufferSize = 1400;
	FAsioTcp TCP;
	asio::streambuf RequestBuffer;
	asio::streambuf ResponseBuffer;
	FHandShake Handshake;
	FDataFrame SDataFrame;

	void post_string(const FString& str);
	void post_buffer(EOpcode opcode, const TArray<uint8>& buffer);
	void package_string(const FString& str);
	std::string encode_string_payload(const std::string& payload);
	void package_buffer(const TArray<uint8>& buffer);
	std::vector<uint8> encode_buffer_payload(const std::vector<uint8>& payload);
	size_t get_frame_encode_size(const size_t buffer_size);
	std::array<uint8, 4> mask_gen();
	bool decode_payload(FWsMessage& data_frame);

	void consume_response_buffer()
	{
		RequestBuffer.consume(RequestBuffer.size());
		ResponseBuffer.consume(ResponseBuffer.size());
	}

	void run_context_thread();
	void resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void conn(const std::error_code& error);
	void write_handshake(const std::error_code& error, const size_t bytes_sent);
	void read_handshake(const std::error_code& error, const size_t bytes_sent, const size_t bytes_recvd);
	void consume_header_buffer(const std::error_code& error);
	void write(const std::error_code& error, const size_t bytes_sent);
	void read(const std::error_code& error, const size_t bytes_recvd);
};

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UWebsocketClientSsl : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override
	{
		ShouldStopContext = true;
		TCP.resolver.cancel();
		if (IsConnected()) Close();
		ThreadPool->wait();
		consume_response_buffer();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Remote")
	void SetHost(const FString& url = "localhost", const FString& port = "")
	{
		Host = url;
		Service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Local")
	FString GetLocalAdress() const
	{
		if (IsConnected())
		{
			return TCP.ssl_socket.lowest_layer().local_endpoint().address().to_string().c_str();
		}
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Local")
	FString GetLocalPort() const
	{
		if (IsConnected())
		{
			return FString::FromInt(TCP.ssl_socket.lowest_layer().local_endpoint().port());
		}
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Remote")
	FString GetRemoteAdress() const
	{
		if (IsConnected())
		{
			return TCP.ssl_socket.lowest_layer().remote_endpoint().address().to_string().c_str();
		}
		return Host;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Remote")
	FString GetRemotePort() const
	{
		if (IsConnected())
		{
			return FString::FromInt(TCP.ssl_socket.lowest_layer().remote_endpoint().port());
		}
		return Service;
	}

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Settings")
	void SetTimeout(uint8 value = 3) { Timeout = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Settings")
	uint8 GetTimeout() const { return Timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Settings")
	void SetMaxAttemp(uint8 value = 3) { MaxAttemp = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Settings")
	uint8 GetMaxAttemp() const { return Timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Settings")
	void SetMaxSendBufferSize(int value = 1400) { MaxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Settings")
	void SetSplitPackage(bool value = true) { SplitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Settings")
	bool GetSplitPackage() const { return SplitBuffer; }


	/*HANDSHAKE*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void SetPath(const FString& value = "chat") { Handshake.path = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString GetPath() const { return Handshake.path; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void SetVersion(const FString& value = "1.1") { Handshake.version = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString GetVersion() const { return Handshake.version; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void SetKey(const FString& value = "dGhlIHNhbXBsZSBub25jZQ==") { Handshake.Sec_WebSocket_Key = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString GetKey() const { return Handshake.Sec_WebSocket_Key; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void SetOrigin(const FString& value = "client") { Handshake.origin = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString GetOrigin() const { return Handshake.origin; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void SetSecProtocol(const FString& value = "chat, superchat") { Handshake.Sec_WebSocket_Protocol = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString GetSecProtocol() const { return Handshake.Sec_WebSocket_Protocol; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void SetSecVersion(const FString& value = "13") { Handshake.Sec_Websocket_Version = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString GetSecVersion() const { return Handshake.Sec_Websocket_Version; }

	/*DATAFRAME*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Dataframe")
	void SetRSV1(bool value = false) { SDataFrame.rsv1 = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Dataframe")
	bool UseRSV1() const { return SDataFrame.rsv1; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Dataframe")
	void SetRSV2(bool value = false) { SDataFrame.rsv2 = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Dataframe")
	bool UseRSV2() const { return SDataFrame.rsv2; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Dataframe")
	void SetRSV3(bool value = false) { SDataFrame.rsv3 = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Dataframe")
	bool UseRSV3() const { return SDataFrame.rsv3; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Dataframe")
	void SetMask(bool value = true) { SDataFrame.mask = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Dataframe")
	bool UseMask() const { return SDataFrame.mask; }

	/*SECURITY LAYER*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Security Layer")
	bool LoadPrivateKeyData(const FString& key_data) noexcept
	{
		if (key_data.IsEmpty()) return false;
		asio::error_code ec;
		std::string key = TCHAR_TO_UTF8(*key_data);
		const asio::const_buffer buffer(key.data(), key.size());
		TCP.ssl_context.use_private_key(buffer, asio::ssl::context::pem, ec);
		if (ec)
		{
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR>"));
			UE_LOG(LogTemp, Error, TEXT("%hs"), ec.message().c_str());
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR/>"));
			OnError.Broadcast(ec.value(), ec.message().c_str());
			return false;
		}
		return true;
	}

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Security Layer")
	bool LoadPrivateKeyFile(const FString& filename)
	{
		if (filename.IsEmpty()) return false;
		asio::error_code ec;
		std::string file = TCHAR_TO_UTF8(*filename);
		TCP.ssl_context.use_private_key_file(file, asio::ssl::context::pem, ec);
		if (ec)
		{
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR>"));
			UE_LOG(LogTemp, Error, TEXT("%hs"), ec.message().c_str());
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR/>"));
			OnError.Broadcast(ec.value(), ec.message().c_str());
			return false;
		}
		return true;
	}

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Security Layer")
	bool LoadCertificateData(const FString& cert_data)
	{
		if (cert_data.IsEmpty()) return false;
		asio::error_code ec;
		std::string cert = TCHAR_TO_UTF8(*cert_data);
		const asio::const_buffer buffer(cert.data(), cert.size());
		TCP.ssl_context.use_certificate(buffer, asio::ssl::context::pem);
		if (ec)
		{
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR>"));
			UE_LOG(LogTemp, Error, TEXT("%hs"), ec.message().c_str());
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR/>"));
			OnError.Broadcast(ec.value(), ec.message().c_str());
			return false;
		}
		return true;
	}

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Security Layer")
	bool LoadCertificateFile(const FString& filename)
	{
		if (filename.IsEmpty()) return false;
		asio::error_code ec;
		std::string file = TCHAR_TO_UTF8(*filename);
		TCP.ssl_context.use_certificate_file(file, asio::ssl::context::pem, ec);
		if (ec)
		{
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR>"));
			UE_LOG(LogTemp, Error, TEXT("%hs"), ec.message().c_str());
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR/>"));
			OnError.Broadcast(ec.value(), ec.message().c_str());
			return false;
		}
		return true;
	}

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Security Layer")
	bool LoadCertificateChainData(const FString& cert_chain_data)
	{
		if (cert_chain_data.IsEmpty()) return false;
		asio::error_code ec;
		std::string cert_chain = TCHAR_TO_UTF8(*cert_chain_data);
		const asio::const_buffer buffer(cert_chain.data(),
										cert_chain.size());
		TCP.ssl_context.use_certificate_chain(buffer, ec);
		if (ec)
		{
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR>"));
			UE_LOG(LogTemp, Error, TEXT("%hs"), ec.message().c_str());
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR/>"));
			OnError.Broadcast(ec.value(), ec.message().c_str());
			return false;
		}
		return true;
	}

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Security Layer")
	bool LoadCertificateChainFile(const FString& filename)
	{
		if (filename.IsEmpty()) return false;
		asio::error_code ec;
		std::string file = TCHAR_TO_UTF8(*filename);
		TCP.ssl_context.use_certificate_chain_file(file, ec);
		if (ec)
		{
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR>"));
			UE_LOG(LogTemp, Error, TEXT("%hs"), ec.message().c_str());
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR/>"));
			OnError.Broadcast(ec.value(), ec.message().c_str());
			return false;
		}
		return true;
	}

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Security Layer")
	bool LoadVerifyFile(const FString& filename)
	{
		if (filename.IsEmpty()) return false;
		asio::error_code ec;
		std::string file = TCHAR_TO_UTF8(*filename);
		TCP.ssl_context.load_verify_file(file, ec);
		if (ec)
		{
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR>"));
			UE_LOG(LogTemp, Error, TEXT("%hs"), ec.message().c_str());
			UE_LOG(LogTemp, Error, TEXT("<ASIO ERROR/>"));
			OnError.Broadcast(ec.value(), ec.message().c_str());
			return false;
		}
		return true;
	}

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Message")
	bool Send(const FString& message);
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Message")
	bool SendRaw(const TArray<uint8> buffer);
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Message")
	bool SendPing();
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Message")
	bool AsyncRead();

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Connection")
	bool Connect();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Connection")
	bool IsConnected() const { return TCP.ssl_socket.lowest_layer().is_open(); }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Connection")
	void Close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Error")
	int GetErrorCode() const { return TCP.error_code.value(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Error")
	FString GetErrorMessage() const { return TCP.error_code.message().c_str(); }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsConnection OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsConnectionRetry OnConnectionWillRetry;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsConnection OnPongReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsConnection OnCloseNotify;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateWsError OnError;

private:
	TUniquePtr<asio::thread_pool> ThreadPool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	bool ShouldStopContext = true;
	FString Host = "localhost";
	FString Service;
	uint8 Timeout = 3;
	uint8 MaxAttemp = 3;
	bool SplitBuffer = false;
	int MaxSendBufferSize = 1400;
	FAsioTcpSsl TCP;
	asio::streambuf RequestBuffer;
	asio::streambuf ResponseBuffer;
	FHandShake Handshake;
	FDataFrame SDataFrame;

	void post_string(const FString& str);
	void post_buffer(EOpcode opcode, const TArray<uint8>& buffer);
	void package_string(const FString& str);
	std::string encode_string_payload(const std::string& payload);
	void package_buffer(const TArray<uint8>& buffer);
	std::vector<uint8> encode_buffer_payload(const std::vector<uint8>& payload);
	size_t get_frame_encode_size(const size_t buffer_size);
	std::array<uint8, 4> mask_gen();
	bool decode_payload(FWsMessage& data_frame);

	void consume_response_buffer()
	{
		RequestBuffer.consume(RequestBuffer.size());
		ResponseBuffer.consume(ResponseBuffer.size());
	}

	void run_context_thread();
	void resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void conn(const std::error_code& error);
	void ssl_handshake(const std::error_code& error);
	void write_handshake(const std::error_code& error, const size_t bytes_sent);
	void read_handshake(const std::error_code& error, const size_t bytes_sent, const size_t bytes_recvd);
	void consume_header_buffer(const std::error_code& error);
	void write(const std::error_code& error, const size_t bytes_sent);
	void read(const std::error_code& error, const size_t bytes_recvd);
};