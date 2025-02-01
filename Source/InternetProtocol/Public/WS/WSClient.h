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
#include "Library/InternetProtocolFunctionLibrary.h"
#include "WSClient.generated.h"

/**
 * 
 */

UCLASS(Blueprintable, BlueprintType, Category = "IP|Websocket")
class INTERNETPROTOCOL_API UWSClient : public UObject
{
	GENERATED_BODY()

public:
	UWSClient()
	{
		RequestHandshake.Headers.Add("Connection", "Upgrade");
		RequestHandshake.Headers.Add("Origin", "ASIO");
		RequestHandshake.Headers.Add("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
		RequestHandshake.Headers.Add("Sec-WebSocket-Protocol", "chat, superchat");
		RequestHandshake.Headers.Add("Sec-WebSocket-Version", "13");
		RequestHandshake.Headers.Add("Upgrade", "websocket");
	}

	virtual void BeginDestroy() override
	{
		TCP.resolver.cancel();
		if (TCP.socket.is_open())
		{
			Close();
		}
		consume_response_buffer();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Remote")
	void SetHost(const FString& Url = "localhost", const FString& Port = "3000")
	{
		Host = Url;
		Service = Port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Socket")
	FTCPSocket GetSocket() { return TCP.socket; }

	/*SETTINGS*/

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Settings")
	void SetMaxSendBufferSize(int Value = 1400) { MaxSendBufferSize = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Settings")
	void SetSplitPackage(bool Value = true) { SplitBuffer = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Settings")
	bool GetSplitPackage() const { return SplitBuffer; }


	/*HANDSHAKE*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Handshake")
	void AppendHeader(const FString& Key, const FString& Value) { RequestHandshake.Headers[Key] = Value; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Handshake")
	void ClearHeaders() { UHttpFunctionLibrary::ClearRequest(RequestHandshake); }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Handshake")
	void RemoveHeader(const FString& Key)
	{
		if (!RequestHandshake.Headers.Contains(Key))
		{
			return;
		}
		RequestHandshake.Headers.Remove(Key);
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Handshake")
	bool HasHeader(const FString& Key) { return RequestHandshake.Headers.Contains(Key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Handshake")
	FString GetHeader(const FString& Key) { return RequestHandshake.Headers[Key]; }

	/*DATAFRAME*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Dataframe")
	void SetRSV1(bool Value = false) { SDataFrame.rsv1 = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Dataframe")
	bool UseRSV1() const { return SDataFrame.rsv1; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Dataframe")
	void SetRSV2(bool Value = false) { SDataFrame.rsv2 = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Dataframe")
	bool UseRSV2() const { return SDataFrame.rsv2; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Dataframe")
	void SetRSV3(bool Value = false) { SDataFrame.rsv3 = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Dataframe")
	bool UseRSV3() const { return SDataFrame.rsv3; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Dataframe")
	void SetMask(bool Value = true) { SDataFrame.mask = Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Dataframe")
	bool UseMask() const { return SDataFrame.mask; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Message")
	bool SendStr(const FString& Message);
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Message")
	bool SendBuffer(const TArray<uint8> Buffer);
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Message")
	bool SendPing();

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Connection")
	bool Connect();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Connection")
	void Close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Error")
	FErrorCode GetErrorCode() const { return ErrorCode; }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateHttpClientResponse OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateBytesTransferred OnBytesTransferred;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsMessage OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateConnection OnPongReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateConnection OnCloseNotify;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateHttpDataError OnHandshakeFail;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FAsioTcpClient TCP;
	asio::error_code ErrorCode;
	FString Host = "localhost";
	FString Service = "3000";
	bool SplitBuffer = false;
	int MaxSendBufferSize = 1400;
	asio::streambuf ResponseBuffer;
	FClientRequest RequestHandshake;
	FClientResponse ResponseHandshake;
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
	TArray<uint8> sha1(const FString& input);
	FString base64_encode(const uint8_t* input, size_t length);
	FString generate_accept_key(const FString &sec_websocket_key);
	void consume_response_buffer();
	void run_context_thread();
	void resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void conn(const std::error_code& error);
	void write_handshake(const std::error_code& error, const size_t bytes_sent);
	void read_handshake(const std::error_code& error, const size_t bytes_recvd);
	void read_headers(const std::error_code& error);
	void write(const std::error_code& error, const size_t bytes_sent);
	void read(const std::error_code& error, const size_t bytes_recvd);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|Websocket")
class INTERNETPROTOCOL_API UWSClientSsl : public UObject
{
	GENERATED_BODY()

public:
	UWSClientSsl()
	{
		RequestHandshake.Headers.Add("Connection", "Upgrade");
		RequestHandshake.Headers.Add("Origin", "ASIO");
		RequestHandshake.Headers.Add("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
		RequestHandshake.Headers.Add("Sec-WebSocket-Protocol", "chat, superchat");
		RequestHandshake.Headers.Add("Sec-WebSocket-Version", "13");
		RequestHandshake.Headers.Add("Upgrade", "websocket");
	}

	virtual void BeginDestroy() override
	{
		TCP.resolver.cancel();
		if (TCP.ssl_socket.lowest_layer().is_open())
		{
			Close();
		}
		consume_response_buffer();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Remote")
	void SetHost(const FString& url = "localhost", const FString& port = "3000")
	{
		Host = url;
		Service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Context")
	FSslContext GetContext() { return TCP.ssl_context; }
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Socket")
	FTCPSslSocket GetSocket() { return TCP.ssl_socket; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Socket")
	void UpdateSslSocket() { TCP.ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(TCP.context, TCP.ssl_context); }

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Settings")
	void SetMaxSendBufferSize(int value = 1400) { MaxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Settings")
	void SetSplitPackage(bool value = true) { SplitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Settings")
	bool GetSplitPackage() const { return SplitBuffer; }


	/*HANDSHAKE*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Handshake")
	void AppendHeader(const FString& Key, const FString& Value) { RequestHandshake.Headers[Key] = Value; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Handshake")
	void ClearHeaders() { UHttpFunctionLibrary::ClearRequest(RequestHandshake); }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Handshake")
	void RemoveHeader(const FString& Key)
	{
		if (!RequestHandshake.Headers.Contains(Key))
		{
			return;
		}
		RequestHandshake.Headers.Remove(Key);
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Handshake")
	bool HasHeader(const FString& Key) { return RequestHandshake.Headers.Contains(Key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Handshake")
	FString GetHeader(const FString& Key) { return RequestHandshake.Headers[Key]; }

	/*DATAFRAME*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Dataframe")
	void SetRSV1(bool value = false) { SDataFrame.rsv1 = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Dataframe")
	bool UseRSV1() const { return SDataFrame.rsv1; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Dataframe")
	void SetRSV2(bool value = false) { SDataFrame.rsv2 = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Dataframe")
	bool UseRSV2() const { return SDataFrame.rsv2; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Dataframe")
	void SetRSV3(bool value = false) { SDataFrame.rsv3 = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Dataframe")
	bool UseRSV3() const { return SDataFrame.rsv3; }

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Dataframe")
	void SetMask(bool value = true) { SDataFrame.mask = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Dataframe")
	bool UseMask() const { return SDataFrame.mask; }

	/*SECURITY LAYER*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Security Layer")
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
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Message")
	bool SendStr(const FString& message);
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Message")
	bool SendBuffer(const TArray<uint8> buffer);
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Message")
	bool SendPing();

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Connection")
	bool Connect();

	UFUNCTION(BlueprintCallable, Category = "IP|Websocket|Connection")
	void Close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|Websocket|Error")
	FErrorCode GetErrorCode() const { return ErrorCode; }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateHttpClientResponse OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateBytesTransferred OnBytesTransferred;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateWsMessage OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateConnection OnPongReveived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateConnection OnCloseNotify;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateHttpDataError OnHandshakeFail;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|Websocket|Events")
	FDelegateError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FAsioTcpSslClient TCP;
	asio::error_code ErrorCode;
	FString Host = "localhost";
	FString Service = "3000";
	bool SplitBuffer = false;
	int MaxSendBufferSize = 1400;
	asio::streambuf ResponseBuffer;
	FClientRequest RequestHandshake;
	FClientResponse ResponseHandshake;
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
	void consume_response_buffer();
	TArray<uint8> sha1(const FString& input);
	FString base64_encode(const uint8_t* input, size_t length);
	FString generate_accept_key(const FString &sec_websocket_key);
	void run_context_thread();
	void resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void conn(const std::error_code& error);
	void ssl_handshake(const std::error_code& error);
	void write_handshake(const std::error_code& error, const size_t bytes_sent);
	void read_handshake(const std::error_code& error, const size_t bytes_recvd);
	void read_headers(const std::error_code& error);
	void write(const std::error_code& error, const size_t bytes_sent);
	void read(const std::error_code& error, const size_t bytes_recvd);
};
