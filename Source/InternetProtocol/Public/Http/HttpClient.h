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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateRequestCompleted, const UHeader*, Headers);
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
		if (!response_header)
			response_header = NewObject<UHeader>();

		request.headers["Accept"] = "*/*";
		request.headers["User-Agent"] = "ASIO 2.30.2";
		request.headers["Connection"] = "close";
	}
	~UHttpClient(){}

	/*HTTP SETTINGS*/

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void setHost(const FString& url = "localhost", const FString& port = "")
	{
		host = url;
		service = port;
	}
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	FString getHost() const { return host; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	FString getPort() const { return service; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void setRetryTime(int value = 3) { retrytime = value; }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void resetRetryTime() { retrytime = 3; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	int getRetryTime() const { return retrytime; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void setMaxRetry(int value = 3) { maxretry = value; }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void resetMaxRetry() { maxretry = 3; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	int getMaxRetry() const { return maxretry; }
	
	/*REQUEST DATA*/
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void setRequest(const FRequest& value) { request = value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	FRequest getRequest() const { return request; }
	
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void setRequestMethod(EVerb requestMethod = EVerb::GET) { request.verb = requestMethod; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	EVerb getRequestMethod() const { return request.verb; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void setVersion(const FString& version = "2.0")	{ request.version = version; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	FString getVersion() const { return request.version; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void setPath(const FString& path = "/") { request.path = path.IsEmpty() ? "/" : path; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	FString getPath() const { return request.path; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void appendParams(const FString& key, const FString& value) { request.params[key] = value; }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void clearParams() { request.params.Empty(); }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void removeParam(const FString& key) { request.params.Remove(key); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	bool hasParam(const FString& key) const { return request.params.Contains(key); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	TMap<FString, FString> getParams() const { return request.params; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void AppendHeaders(const FString& key, const FString& value) { request.headers[key] = value; }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void ClearHeaders() { request.headers.Empty(); }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void RemoveHeader(const FString& key) { request.headers.Remove(key); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	bool hasHeader(const FString& key) const { return request.headers.Contains(key); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	TMap<FString, FString> GetHeaders() const { return request.headers; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void SetBody(const FString& value) { request.body = value; }
	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	void ClearBody() { request.body.Empty(); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||HTTP")
	FString GetBody() const { return request.body; }

	UFUNCTION(BlueprintCallable, Category = "IP||HTTP")
	FString getData();

	/*THREADS*/
	void setThreadNumbers(int value = 2) { pool = MakeUnique<asio::thread_pool>(2); }

	/*PAYLOAD*/
	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
	void preparePayload();
	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
	int async_preparePayload();
	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
	void clearPayload() { payload.Empty(); }
	
	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
	int processRequest();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
	int getErrorCode() const { return asio.error_code.value(); }
	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
	FString getErrorMessage() const { return asio.error_code.message().data(); }

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
	UHeader* response_header;
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
