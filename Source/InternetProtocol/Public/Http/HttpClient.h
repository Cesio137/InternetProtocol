// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/Net/Commons.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "HttpClient.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateRequestConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateRequestCompleted, const FResponse, Headers);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateRequestRetry, int, Attemps, int, TimeToRetry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateRequestError, int, Code, const FString&, exception);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateProgress, FRequest, Request, int, BytesSent, int, BytesReceived);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UHttpClient : public UObject
{
	GENERATED_BODY()
public:
	UHttpClient()
	{
		request.headers.Add("Accept", "*/*");
		request.headers.Add("User-Agent", "ASIO 2.30.2");
		request.headers.Add("Connection", "close");
	}
	~UHttpClient()
	{
		clearRequest();
		clearPayload();
		clearResponse();
	}

	/*HTTP SETTINGS*/

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Settings")
	void setHost(const FString& url = "localhost", const FString& port = "")
	{
		host = url;
		service = port;
	}
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Settings")
	FString getHost() const { return host; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Settings")
	FString getPort() const { return service; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Settings")
	void setRetryTime(int value = 3) { retrytime = value; }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Settings")
	void resetRetryTime() { retrytime = 3; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Settings")
	int getRetryTime() const { return retrytime; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||SETTINGS")
	void setMaxRetry(int value = 3) { maxretry = value; }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||SETTINGS")
	void resetMaxRetry() { maxretry = 3; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||SETTINGS")
	int getMaxRetry() const { return maxretry; }
	
	/*REQUEST DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void setRequest(const FRequest& value) { request = value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	FRequest getRequest() const { return request; }
	
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void setRequestMethod(EVerb requestMethod = EVerb::GET) { request.verb = requestMethod; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	EVerb getRequestMethod() const { return request.verb; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void setVersion(const FString& version = "2.0")	{ request.version = version; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	FString getVersion() const { return request.version; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void setPath(const FString& path = "/") { request.path = path.IsEmpty() ? "/" : path; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	FString getPath() const { return request.path; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void appendParams(const FString& key, const FString& value) { request.params.Add(key, value); }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void clearParams() { request.params.Empty(); }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void removeParam(const FString& key) { request.params.Remove(key); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	bool hasParam(const FString& key) const { return request.params.Contains(key); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	TMap<FString, FString> getParams() const { return request.params; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void AppendHeaders(const FString& key, const FString& value) { request.headers.Add(key, value); }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void ClearHeaders() { request.headers.Empty(); }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void RemoveHeader(const FString& key) { request.headers.Remove(key); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	bool hasHeader(const FString& key) const { return request.headers.Contains(key); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	TMap<FString, FString> GetHeaders() const { return request.headers; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void SetBody(const FString& value) { request.body = value; }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	void ClearBody() { request.body.Empty(); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP||Request")
	FString GetBody() const { return request.body; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Request")
	FRequest getRequestData() const { return request; }

	/*PAYLOAD*/
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Payload")
	void preparePayload();
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Payload")
	int async_preparePayload();
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Payload")
	FString getPayloadData() const { return payload; }

	/*RESPONSE DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Payload")
	FResponse getResponseData() const { return response; }
	
	/*THREADS*/
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Thread")
	void setThreadNumbers(int value = 2) { pool = MakeUnique<asio::thread_pool>(2); }
	
	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Connection")
	int processRequest();

	/*MEMORY MANAGER*/
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP||Memory")
	void clearRequest() { request.clear(); }
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Memory")
	void clearPayload() { payload.Empty(); }
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Memory")
	void clearResponse() { response.clear(); }

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Error")
	int getErrorCode() const { return asio.error_code.value(); }
	UFUNCTION(BlueprintCallable, Category="IP||HTTP||Error")
	FString getErrorMessage() const { return asio.error_code.message().data(); }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestConnected OnConnected;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestConnected OnAsyncPayloadFinished;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestCompleted OnRequestCompleted;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestRetry OnRequestWillRetry;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateRequestError OnRequestError;
	
private:
	TUniquePtr<asio::thread_pool> pool = MakeUnique<asio::thread_pool>(2);
	std::mutex mutexPayload;
	std::mutex mutexIO;
	void runContextThread();
	
	FString host = "localhost";
	FString service;
	int maxretry = 1;
	int retrytime = 3;
	FRequest request;
	FAsio asio;
	FString payload;
	asio::streambuf request_buffer;
	asio::streambuf response_buffer;
	FResponse response;
	const TMap<EVerb, FString> verb = {
		{EVerb::GET     , "GET"},
		{EVerb::POST    , "POST"},
		{EVerb::PUT     , "PUT"},
		{EVerb::PATCH   , "PATCH"},
		{EVerb::DEL     , "DELETE"},
		{EVerb::COPY    , "COPY"},
		{EVerb::HEAD    , "HEAD"},
		{EVerb::OPTIONS , "OPTIONS"},
		{EVerb::LOCK    , "LOCK"},
		{EVerb::UNLOCK  , "UNLOCK"},
		{EVerb::PROPFIND, "PROPFIND"},
	};

	void resolve(const std::error_code& err, const asio::ip::tcp::resolver::results_type& endpoints);
	void connect(const std::error_code& err);
	void write_request(const std::error_code& err);
	void read_status_line(const std::error_code& err);
	void read_headers(const std::error_code& err);
	void read_content(const std::error_code& err);
	
};
