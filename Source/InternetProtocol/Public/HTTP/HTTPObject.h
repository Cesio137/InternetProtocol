// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "Library/StructLibrary.h"
#include "HTTPObject.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateResponse, FRequest, Request, FResponse, Response, bool, Success);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateRetry, FRequest, Request, FResponse, Response, float, TimeToRetry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateProgress, FRequest, Request, int, BytesSent, int, BytesReceived);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UHTTPObject : public UObject
{
	GENERATED_BODY()
public:
	virtual void BeginDestroy() override;
	
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> GetHttp();
public:
	UFUNCTION(BlueprintCallable, Category = "HTTP", meta=(ExpandEnumAsExecs = "Output"))
	void ConstructHttp(TEnumAsByte<EOutputExecPins>& Output);

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void Reset();

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void SetUrl(const FString& URL);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HTTP")
	const FString GetUrl();

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	virtual void SetVerb(const EVerbMode Verb);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HTTP")
	const FString GetVerb();

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void SetParameters(const TMap<FString, FString>& Parameters);

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	const FString GetParameter(const FString& ParameterName);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HTTP")
	const TMap<FString, FString> GetAllParameters();

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void AppendToHeader(const TMap<FString, FString>& Headers);

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	const FString GetHeader(const FString& HeaderName);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HTTP")
	const TArray<FString> GetAllHeaders();

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void SetContent(const TArray<uint8>& Content);

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void SetContentAsString(const FString& Content);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HTTP")
	const TArray<uint8> GetContent();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HTTP")
	const int GetContentLenght();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HTTP")
	const FString GetContentType();

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void SetTimeOut(const float InTimeoutSecs = 3.0f);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HTTP")
	float GetTimeOut();

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void ClearTimeOut();

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void Tick(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "HTTP", meta=(ExpandEnumAsExecs = "Output"))
	void ProcessRequest(TEnumAsByte<EOutputExecPins>& Output);

	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void CancelRequest();

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "HTTP||Events")
	FDelegateResponse OnComplete;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "HTTP||Events")
	FDelegateProgress OnProgress;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "HTTP||Events")
	FDelegateRetry OnWillRetry;

private:
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest;
	FString Url;
	TMap<FString, FString> UrlParameters;
	float TimeoutSecs = 3.0f;
};
