// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Net/Commons.h"
#include "Library/InternetProtocolStructLibrary.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "HttpClient.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateRequest);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateRequestCompleted, const FResponse, headers);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateRequestRetry, const int, attemps);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateRequestError, const int, code, const FString&, exception);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateResponseError, const int, statusCode);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateRequestProgress, const int, bytesSent, const int, bytesReceived);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateHttpError, const int, code, const FString&, exception);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UHttpClient : public UObject
{
	GENERATED_BODY()

public:
	UHttpClient()
	{
		Request.headers.Add("Accept", "*/*");
		Request.headers.Add("User-Agent", "ASIO 2.30.2");
		Request.headers.Add("Connection", "close");
	}

	virtual void BeginDestroy() override
	{
		ShouldStopContext = true;
		TCP.resolver.cancel();
		if (!TCP.context.stopped() || TCP.socket.is_open()) CancelRequest();
		ThreadPool->wait();
		ClearRequest();
		ClearPayload();
		ClearResponse();
		Super::BeginDestroy();
	}

	/*HTTP SETTINGS*/

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Settings")
	void SetHost(const FString& url = "localhost", const FString& port = "")
	{
		Host = url;
		Service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Settings")
	FString GetHost() const { return Host; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Settings")
	FString GetPort() const { return Service; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Settings")
	void SetTimeout(int value = 3) { Timeout = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Settings")
	int GetTimeout() const { return Timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||SETTINGS")
	void SetMaxAttemp(int value = 3) { MaxAttemp = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||SETTINGS")
	int GetMaxAttemp() const { return MaxAttemp; }

	/*REQUEST DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void SetRequest(const FRequest& value) { Request = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	FRequest GetRequest() const { return Request; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void SetRequestMethod(EVerb requestMethod = EVerb::GET) { Request.verb = requestMethod; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	EVerb GetRequestMethod() const { return Request.verb; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void SetVersion(const FString& version = "1.1") { Request.version = version; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	FString GetVersion() const { return Request.version; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void SetPath(const FString& path = "/") { Request.path = path.IsEmpty() ? "/" : path; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	FString GetPath() const { return Request.path; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void AppendParams(const FString& key, const FString& value) { Request.params.Add(key, value); }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void ClearParams() { Request.params.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void RemoveParam(const FString& key) { Request.params.Remove(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	bool HasParam(const FString& key) const { return Request.params.Contains(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	TMap<FString, FString> GetParams() const { return Request.params; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void AppendHeaders(const FString& key, const FString& value) { Request.headers.Add(key, value); }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void ClearHeaders() { Request.headers.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void RemoveHeader(const FString& key) { Request.headers.Remove(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	bool HasHeader(const FString& key) const { return Request.headers.Contains(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	TMap<FString, FString> GetHeaders() const { return Request.headers; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void SetBody(const FString& value) { Request.body = value; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void ClearBody() { Request.body.Empty(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	FString GetBody() const { return Request.body; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	FRequest GetRequestData() const { return Request; }

	/*PAYLOAD*/
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Payload")
	void PreparePayload();
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Payload")
	bool AsyncPreparePayload();
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Payload")
	FString GetPayloadData() const { return Payload; }

	/*RESPONSE DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Response")
	FResponse GetResponseData() const { return Response; }

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Connection")
	bool ProcessRequest();

	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Connection")
	void CancelRequest();

	/*MEMORY MANAGER*/
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Memory")
	void ClearRequest() { Request.clear(); }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Memory")
	void ClearPayload() { Payload.Empty(); }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Memory")
	void ClearResponse() { Response.clear(); }

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Error")
	int GetErrorCode() const { return TCP.error_code.value(); }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Error")
	FString GetErrorMessage() const { return TCP.error_code.message().c_str(); }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequest OnAsyncPayloadFinished;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestCompleted OnRequestCompleted;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestProgress OnRequestProgress;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestRetry OnRequestWillRetry;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestError OnRequestError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequest OnRequestCanceled;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateResponseError OnResponseError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateHttpError OnError;

private:
	TUniquePtr<asio::thread_pool> ThreadPool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	FCriticalSection MutexIO;
	FCriticalSection MutexPayload;
	bool ShouldStopContext = false;
	FString Host = "localhost";
	FString Service;
	uint8 Timeout = 3;
	uint8 MaxAttemp = 3;
	FRequest Request;
	FAsioTcp TCP;
	FString Payload;
	asio::streambuf RequestBuffer;
	asio::streambuf ResponseBuffer;
	FResponse Response;
	const TMap<EVerb, FString> Verb = {
		{EVerb::GET, "GET"},
		{EVerb::POST, "POST"},
		{EVerb::PUT, "PUT"},
		{EVerb::PATCH, "PATCH"},
		{EVerb::DEL, "DELETE"},
		{EVerb::COPY, "COPY"},
		{EVerb::HEAD, "HEAD"},
		{EVerb::OPTIONS, "OPTIONS"},
		{EVerb::LOCK, "LOCK"},
		{EVerb::UNLOCK, "UNLOCK"},
		{EVerb::PROPFIND, "PROPFIND"},
	};

	void consume_stream_buffers()
	{
		RequestBuffer.consume(RequestBuffer.size());
		ResponseBuffer.consume(RequestBuffer.size());
	}

	void run_context_thread();
	void resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints) noexcept;
	void connect(const std::error_code& error) noexcept;
	void write_request(const std::error_code& error, const size_t bytes_sent) noexcept;
	void read_status_line(const std::error_code& error, const size_t bytes_sent, const size_t bytes_recvd) noexcept;
	void read_headers(const std::error_code& error) noexcept;
	void read_content(const std::error_code& error) noexcept;
};

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UHttpClientSsl : public UObject
{
	GENERATED_BODY()

public:
	UHttpClientSsl()
	{
		Request.headers.Add("Accept", "*/*");
		Request.headers.Add("User-Agent", "ASIO 2.30.2");
		Request.headers.Add("Connection", "close");
	}

	virtual void BeginDestroy() override
	{
		ShouldStopContext = true;
		TCP.resolver.cancel();
		if (!TCP.context.stopped() || TCP.ssl_socket.lowest_layer().is_open()) CancelRequest();
		ThreadPool->wait();
		ClearRequest();
		ClearPayload();
		ClearResponse();
		Super::BeginDestroy();
	}

	/*HTTP SETTINGS*/

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Settings")
	void SetHost(const FString& url = "localhost", const FString& port = "")
	{
		Host = url;
		Service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Settings")
	FString GetHost() const { return Host; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Settings")
	FString GetPort() const { return Service; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Settings")
	void SetTimeout(int value = 3) { Timeout = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Settings")
	int GetTimeout() const { return Timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||SETTINGS")
	void SetMaxAttemp(int value = 3) { MaxAttemp = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||SETTINGS")
	int GetMaxAttemp() const { return MaxAttemp; }

	/*REQUEST DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void SetRequest(const FRequest& value) { Request = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	FRequest GetRequest() const { return Request; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void SetRequestMethod(EVerb requestMethod = EVerb::GET) { Request.verb = requestMethod; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	EVerb GetRequestMethod() const { return Request.verb; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void SetVersion(const FString& version = "1.1") { Request.version = version; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	FString GetVersion() const { return Request.version; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void SetPath(const FString& path = "/") { Request.path = path.IsEmpty() ? "/" : path; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	FString GetPath() const { return Request.path; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void AppendParams(const FString& key, const FString& value) { Request.params.Add(key, value); }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void ClearParams() { Request.params.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void RemoveParam(const FString& key) { Request.params.Remove(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	bool HasParam(const FString& key) const { return Request.params.Contains(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	TMap<FString, FString> GetParams() const { return Request.params; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void AppendHeaders(const FString& key, const FString& value) { Request.headers.Add(key, value); }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void ClearHeaders() { Request.headers.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void RemoveHeader(const FString& key) { Request.headers.Remove(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	bool HasHeader(const FString& key) const { return Request.headers.Contains(key); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	TMap<FString, FString> GetHeaders() const { return Request.headers; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void SetBody(const FString& value) { Request.body = value; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void ClearBody() { Request.body.Empty(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	FString GetBody() const { return Request.body; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	FRequest GetRequestData() const { return Request; }

	/*SECURITY LAYER*/
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Security Layer")
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

	/*PAYLOAD*/
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Payload")
	void PreparePayload();
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Payload")
	bool AsyncPreparePayload();
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Payload")
	FString GetPayloadData() const { return Payload; }

	/*RESPONSE DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Response")
	FResponse GetResponseData() const { return Response; }

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Connection")
	bool ProcessRequest();

	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Connection")
	void CancelRequest();

	/*MEMORY MANAGER*/
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Memory")
	void ClearRequest() { Request.clear(); }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Memory")
	void ClearPayload() { Payload.Empty(); }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Memory")
	void ClearResponse() { Response.clear(); }

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Error")
	int GetErrorCode() const { return TCP.error_code.value(); }

	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Error")
	FString GetErrorMessage() const { return TCP.error_code.message().c_str(); }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequest OnAsyncPayloadFinished;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestCompleted OnRequestCompleted;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestProgress OnRequestProgress;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestRetry OnRequestWillRetry;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestError OnRequestError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequest OnRequestCanceled;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateResponseError OnResponseError;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateHttpError OnError;

private:
	TUniquePtr<asio::thread_pool> ThreadPool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	FCriticalSection MutexIO;
	FCriticalSection MutexPayload;
	bool ShouldStopContext = false;
	FString Host = "localhost";
	FString Service;
	uint8 Timeout = 3;
	uint8 MaxAttemp = 3;
	FRequest Request;
	FAsioTcpSsl TCP;
	FString Payload;
	asio::streambuf RequestBuffer;
	asio::streambuf ResponseBuffer;
	FResponse Response;
	const TMap<EVerb, FString> Verb = {
		{EVerb::GET, "GET"},
		{EVerb::POST, "POST"},
		{EVerb::PUT, "PUT"},
		{EVerb::PATCH, "PATCH"},
		{EVerb::DEL, "DELETE"},
		{EVerb::COPY, "COPY"},
		{EVerb::HEAD, "HEAD"},
		{EVerb::OPTIONS, "OPTIONS"},
		{EVerb::LOCK, "LOCK"},
		{EVerb::UNLOCK, "UNLOCK"},
		{EVerb::PROPFIND, "PROPFIND"},
	};

	void consume_stream_buffers()
	{
		RequestBuffer.consume(RequestBuffer.size());
		ResponseBuffer.consume(RequestBuffer.size());
	}

	void run_context_thread();
	void resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void connect(const std::error_code& error);
	void ssl_handshake(const std::error_code& error);
	void write_request(const std::error_code& error, const size_t bytes_sent);
	void read_status_line(const std::error_code& error, const size_t bytes_sent, const size_t bytes_recvd);
	void read_headers(const std::error_code& error);
	void read_content(const std::error_code& error);
};