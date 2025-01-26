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
#include "Net/Commons.h"
#include "Library/InternetProtocolFunctionLibrary.h"
#include "HttpClient.generated.h"

/**
 * 
 */

UCLASS(Blueprintable, BlueprintType, Category = "IP|HTTP")
class INTERNETPROTOCOL_API UHttpClient : public UObject
{
	GENERATED_BODY()

public:
	UHttpClient()
	{
		Request.Headers.Add("Accept", "*/*");
		Request.Headers.Add("User-Agent", "ASIO");
		Request.Headers.Add("Connection", "close");
	}

	virtual void BeginDestroy() override
	{
		if (TCP.socket.is_open()) Close();
		Super::BeginDestroy();
	}

	/*HOST*/

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Remote")
	void SetHost(const FString& url = "localhost", const FString& port = "3000")
	{
		Host = url;
		Service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Socket")
	FTCPSocket GetSocket() { return TCP.socket; }

	/*REQUEST DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void SetRequest(const FClientRequest& value) { Request = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	FClientRequest GetRequest() const { return Request; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void SetRequestMethod(EMethod requestMethod = EMethod::GET) { Request.Method = requestMethod; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	EMethod GetRequestMethod() const { return Request.Method; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void SetVersion(const FString& version = "1.1") { Request.Version = version; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	FString GetVersion() const { return Request.Version; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void SetPath(const FString& path = "/") { Request.Path = path.IsEmpty() ? "/" : path; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	FString GetPath() const { return Request.Path; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void AppendParams(const FString& key, const FString& value) { Request.Params.Add(key, value); }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void ClearParams() { Request.Params.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void RemoveParam(const FString& key) { Request.Params.Remove(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	bool HasParam(const FString& key) const { return Request.Params.Contains(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	TMap<FString, FString> GetParams() const { return Request.Params; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void AppendHeaders(const FString& key, const FString& value) { Request.Headers.Add(key, value); }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void ClearHeaders() { Request.Headers.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void RemoveHeader(const FString& key) { Request.Headers.Remove(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	bool HasHeader(const FString& key) const { return Request.Headers.Contains(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	TMap<FString, FString> GetHeaders() const { return Request.Headers; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void SetBody(const FString& value) { Request.Body = value; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void ClearBody() { Request.Body.Empty(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	FString GetBody() const { return Request.Body; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	FClientRequest GetRequestData() const { return Request; }

	/*PAYLOAD*/
	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Payload")
	void PreparePayload();
	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Payload")
	bool AsyncPreparePayload();
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Payload")
	FString GetPayloadData() const { return Payload; }

	/*RESPONSE DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Response")
	FClientResponse GetResponseData() const { return Response; }

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Connection")
	bool ProcessRequest();

	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Connection")
	void Close();

	/*MEMORY MANAGER*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Memory")
	void ClearRequest() {  UHttpFunctionLibrary::ClearRequest(Request); }

	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Memory")
	void ClearPayload() { Payload.Empty(); }

	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Memory")
	void ClearResponse() { UHttpFunctionLibrary::ClientClearResponse(Response); }

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Error")
	FErrorCode GetErrorCode() const { return ErrorCode; }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateConnection OnAsyncPayloadFinished;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateBytesTransferred OnRequestProgress;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpClientResponse OnRequestCompleted;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpDataError OnResponseError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexPayload;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FString Host = "localhost";
	FString Service = "3000";
	FAsioTcpClient TCP;
	asio::error_code ErrorCode;
	FClientRequest Request;
	FString Payload;
	asio::streambuf RequestBuffer;
	asio::streambuf ResponseBuffer;
	FClientResponse Response;

	void consume_stream_buffers();
	void run_context_thread();
	void resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void connect(const std::error_code& error);
	void write_request(const std::error_code& error, const size_t bytes_sent, const bool trigger_read_until);
	void read_status_line(const std::error_code& error, const size_t bytes_recvd);
	void read_headers(const std::error_code& error);
	void read_body(const std::error_code& error);
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|HTTP")
class INTERNETPROTOCOL_API UHttpClientSsl : public UObject
{
	GENERATED_BODY()

public:
	UHttpClientSsl()
	{
		Request.Headers.Add("Accept", "*/*");
		Request.Headers.Add("User-Agent", "ASIO");
		Request.Headers.Add("Connection", "close");
	}

	virtual void BeginDestroy() override
	{
		if (TCP.ssl_socket.lowest_layer().is_open()) Close();
		Super::BeginDestroy();
	}

	/*HOST*/

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Remote")
	void SetHost(const FString& url = "localhost", const FString& port = "3000")
	{
		Host = url;
		Service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Context")
	FSslContext GetContext() { return TCP.ssl_context; }
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Socket")
	FTCPSslSocket GetSocket() { return TCP.ssl_socket; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Socket")
	void UpdateSslSocket() { TCP.ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>(TCP.context, TCP.ssl_context); }

	/*REQUEST DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void SetRequest(const FClientRequest& value) { Request = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	FClientRequest GetRequest() const { return Request; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void SetRequestMethod(EMethod requestMethod = EMethod::GET) { Request.Method = requestMethod; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	EMethod GetRequestMethod() const { return Request.Method; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void SetVersion(const FString& version = "1.1") { Request.Version = version; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	FString GetVersion() const { return Request.Version; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void SetPath(const FString& path = "/") { Request.Path = path.IsEmpty() ? "/" : path; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	FString GetPath() const { return Request.Path; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void AppendParams(const FString& key, const FString& value) { Request.Params.Add(key, value); }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void ClearParams() { Request.Params.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void RemoveParam(const FString& key) { Request.Params.Remove(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	bool HasParam(const FString& key) const { return Request.Params.Contains(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	TMap<FString, FString> GetParams() const { return Request.Params; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void AppendHeaders(const FString& key, const FString& value) { Request.Headers.Add(key, value); }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void ClearHeaders() { Request.Headers.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void RemoveHeader(const FString& key) { Request.Headers.Remove(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	bool HasHeader(const FString& key) const { return Request.Headers.Contains(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	TMap<FString, FString> GetHeaders() const { return Request.Headers; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void SetBody(const FString& value) { Request.Body = value; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	void ClearBody() { Request.Body.Empty(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP|Request")
	FString GetBody() const { return Request.Body; }

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Request")
	FClientRequest GetRequestData() const { return Request; }

	/*SECURITY LAYER*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Security Layer")
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

	/*PAYLOAD*/
	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Payload")
	void PreparePayload();
	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Payload")
	bool AsyncPreparePayload();
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Payload")
	FString GetPayloadData() const { return Payload; }

	/*RESPONSE DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Response")
	FClientResponse GetResponseData() const { return Response; }

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Connection")
	bool ProcessRequest();

	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Connection")
	void Close();

	/*MEMORY MANAGER*/
	UFUNCTION(BlueprintCallable, Category = "IP|HTTP|Memory")
	void ClearRequest() { UHttpFunctionLibrary::ClearRequest(Request); }

	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Memory")
	void ClearPayload() { Payload.Empty(); }

	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Memory")
	void ClearResponse() { UHttpFunctionLibrary::ClientClearResponse(Response); }

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, Category="IP|HTTP|Error")
	FErrorCode GetErrorCode() const { return ErrorCode; }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateConnection OnAsyncPayloadFinished;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateBytesTransferred OnRequestProgress;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpClientResponse OnRequestCompleted;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateHttpDataError OnResponseError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP|HTTP|Events")
	FDelegateError OnError;

private:
	FCriticalSection MutexIO;
	FCriticalSection MutexPayload;
	FCriticalSection MutexError;
	bool IsClosing = false;
	FString Host = "localhost";
	FString Service = "3000";
	asio::error_code ErrorCode;
	FAsioTcpSslClient TCP;
	FClientRequest Request;
	FString Payload;
	asio::streambuf RequestBuffer;
	asio::streambuf ResponseBuffer;
	FClientResponse Response;

	void consume_stream_buffers()
	{
		RequestBuffer.consume(RequestBuffer.size());
		ResponseBuffer.consume(RequestBuffer.size());
	}

	void run_context_thread();
	void resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void connect(const std::error_code& error);
	void ssl_handshake(const std::error_code& error);
	void write_request(const std::error_code& error, const size_t bytes_sent, const bool trigger_read_until);
	void read_status_line(const std::error_code& error, const size_t bytes_recvd);
	void read_headers(const std::error_code& error);
	void read_body(const std::error_code& error);
};