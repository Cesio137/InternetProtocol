// Fill out your copyright notice in the Description page of Project Settings.


#include "HTTP/HTTPObject.h"


void UHTTPObject::BeginDestroy()
{
	UObject::BeginDestroy();
	HttpRequest.Reset();
}

TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> UHTTPObject::GetHttp()
{
	return HttpRequest;
}

void UHTTPObject::ConstructHttp(TEnumAsByte<EOutputExecPins>& Output)
{
	HttpRequest = FHttpModule::Get().CreateRequest();
	if (!HttpRequest.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
	HttpRequest->SetTimeout(TimeoutSecs);
	HttpRequest->OnProcessRequestComplete().BindLambda([&](FHttpRequestPtr Req, FHttpResponsePtr Res, bool Success) 
	{
			FRequest request;
			request.ElapsedTime = Req->GetElapsedTime();
			ERequestStatus requestStatus = ERequestStatus::NotStarted;
			switch (Req->GetStatus())
			{
			case EHttpRequestStatus::NotStarted:
				requestStatus = ERequestStatus::NotStarted;
				break;
			case EHttpRequestStatus::Processing:
				requestStatus = ERequestStatus::Processing;
				break;
			case EHttpRequestStatus::Failed:
				requestStatus = ERequestStatus::Failed;
				break;
			case EHttpRequestStatus::Failed_ConnectionError:
				requestStatus = ERequestStatus::Failed_ConnectionError;
				break;
			case EHttpRequestStatus::Succeeded:
				requestStatus = ERequestStatus::Succeeded;
				break;
			default:
				requestStatus = ERequestStatus::NotStarted;
				break;
			}
			request.RequestStatus = requestStatus;
			FResponse response;
			response.ResponseCode = Res->GetResponseCode();
			response.Content = Res->GetContent();
			response.ContentAsString = Res->GetContentAsString();
			OnComplete.Broadcast(request, response, Success);
	});
	HttpRequest->OnRequestProgress().BindLambda([&](FHttpRequestPtr Req, int32 BytesSent, int32 BytesReceived) 
	{
			FRequest request;
			request.ElapsedTime = Req->GetElapsedTime();
			ERequestStatus requestStatus = ERequestStatus::NotStarted;
			switch (Req->GetStatus())
			{
			case EHttpRequestStatus::NotStarted:
				requestStatus = ERequestStatus::NotStarted;
				break;
			case EHttpRequestStatus::Processing:
				requestStatus = ERequestStatus::Processing;
				break;
			case EHttpRequestStatus::Failed:
				requestStatus = ERequestStatus::Failed;
				break;
			case EHttpRequestStatus::Failed_ConnectionError:
				requestStatus = ERequestStatus::Failed_ConnectionError;
				break;
			case EHttpRequestStatus::Succeeded:
				requestStatus = ERequestStatus::Succeeded;
				break;
			default:
				requestStatus = ERequestStatus::NotStarted;
				break;
			}
			request.RequestStatus = requestStatus;
			OnProgress.Broadcast(request, BytesSent, BytesReceived);
	});
	HttpRequest->OnRequestWillRetry().BindLambda([&](FHttpRequestPtr Req, FHttpResponsePtr Res, float TimeToRetrySecs)
	{
			FRequest request;
			request.ElapsedTime = Req->GetElapsedTime();
			ERequestStatus requestStatus = ERequestStatus::NotStarted;
			switch (Req->GetStatus())
			{
			case EHttpRequestStatus::NotStarted:
				requestStatus = ERequestStatus::NotStarted;
				break;
			case EHttpRequestStatus::Processing:
				requestStatus = ERequestStatus::Processing;
				break;
			case EHttpRequestStatus::Failed:
				requestStatus = ERequestStatus::Failed;
				break;
			case EHttpRequestStatus::Failed_ConnectionError:
				requestStatus = ERequestStatus::Failed_ConnectionError;
				break;
			case EHttpRequestStatus::Succeeded:
				requestStatus = ERequestStatus::Succeeded;
				break;
			default:
				requestStatus = ERequestStatus::NotStarted;
				break;
			}
			request.RequestStatus = requestStatus;
			FResponse response;
			response.ResponseCode = Res->GetResponseCode();
			response.Content = Res->GetContent();
			response.ContentAsString = Res->GetContentAsString();
			OnWillRetry.Broadcast(request, response, TimeToRetrySecs);
	});
	Output = EOutputExecPins::Success;
}

void UHTTPObject::Reset()
{
	HttpRequest.Reset();
	Url.Empty();
	UrlParameters.Empty();
}

void UHTTPObject::SetUrl(const FString& URL)
{
	if (!HttpRequest.IsValid())
		return;

	Url = URL;
	HttpRequest->SetURL(URL);
}

const FString UHTTPObject::GetUrl()
{
	if (!HttpRequest.IsValid())
		return "";
	
	return HttpRequest->GetURL();
}

void UHTTPObject::SetVerb(const EVerbMode Verb)
{
	if (!HttpRequest.IsValid())
		return;

	FString verb = "";
	
	switch (Verb)
	{
	case EVerbMode::GET:
		verb = "GET";
		break;
	case EVerbMode::POST:
		verb = "POST";
		break;
	case EVerbMode::PUT:
		verb = "PUT";
		break;
	case EVerbMode::PATCH:
		verb = "PATCH";
		break;
	case EVerbMode::DEL:
		verb = "DELETE";
		break;
	case EVerbMode::COPY:
		verb = "COPY";
		break;
	case EVerbMode::HEAD:
		verb = "HEAD";
		break;
	case EVerbMode::OPTIONS:
		verb = "OPTIONS";
		break;
	case EVerbMode::LINK:
		verb = "LINK";
		break;
	case EVerbMode::UNLINK:
		verb = "UNLINK";
		break;
	case EVerbMode::LOCK:
		verb = "LOCK";
		break;
	case EVerbMode::UNLOCK:
		verb = "UNLOCK";
		break;
	case EVerbMode::PROPFIND:
		verb = "PROPFIND";
		break;
	case EVerbMode::VIEW:
		verb = "VIEW";
	}

	HttpRequest->SetVerb(verb);
}

const FString UHTTPObject::GetVerb()
{
	if (!HttpRequest.IsValid())
		return "";
	
	return HttpRequest->GetVerb();
}

void UHTTPObject::SetParameters(const TMap<FString, FString>& Parameters)
{
	if (!HttpRequest.IsValid())
		return;

	if (Parameters.Num() <= 0)
		return;

	UrlParameters = Parameters;
	int Index = 0;
	FString UrlParams = Url;
	for (TPair<FString, FString> Parameter : UrlParameters)
	{
		if (Index == 0) 
			UrlParams += "?";
		else 
			UrlParams += "&";

		UrlParams += Parameter.Key + "=" + Parameter.Value;
		Index += 1;
	}

	HttpRequest->SetURL(UrlParams);
}

const FString UHTTPObject::GetParameter(const FString& ParameterName)
{
	if (!HttpRequest.IsValid())
		return "";
	
	return HttpRequest->GetURLParameter(ParameterName);
}

const TMap<FString, FString> UHTTPObject::GetAllParameters()
{
	return UrlParameters;
}

void UHTTPObject::AppendToHeader(const TMap<FString, FString>& Headers)
{
	if (!HttpRequest.IsValid())
		return;

	if (Headers.Num() <= 0 )
		return;

	for (TPair<FString, FString> Header : Headers)
	{
		HttpRequest->AppendToHeader(Header.Key, Header.Value);
	}
}

const FString UHTTPObject::GetHeader(const FString& HeaderName)
{
	if (!HttpRequest.IsValid())
		return "";
	
	return HttpRequest->GetHeader(HeaderName);
}

const TArray<FString> UHTTPObject::GetAllHeaders()
{
	if (!HttpRequest.IsValid())
		return TArray<FString>();
	
	return HttpRequest->GetAllHeaders();
}

void UHTTPObject::SetContent(const TArray<uint8>& Content)
{
	if (!HttpRequest.IsValid())
		return;
	HttpRequest->SetContent(Content);
}

void UHTTPObject::SetContentAsString(const FString& Content)
{
	if (!HttpRequest.IsValid())
		return;
	HttpRequest->SetContentAsString(Content);
}

const TArray<uint8> UHTTPObject::GetContent()
{
	if (!HttpRequest.IsValid())
		return TArray<uint8>();
	return HttpRequest->GetContent();
}

const int UHTTPObject::GetContentLenght()
{
	if (!HttpRequest.IsValid())
		return 0;
	return HttpRequest->GetContentLength();
}

const FString UHTTPObject::GetContentType()
{
	if (!HttpRequest.IsValid())
		return "";
	return HttpRequest->GetContentType();
}

void UHTTPObject::SetTimeOut(const float InTimeoutSecs)
{
	if (!HttpRequest.IsValid())
		return;
	TimeoutSecs = InTimeoutSecs;
	HttpRequest->SetTimeout(InTimeoutSecs);
}

float UHTTPObject::GetTimeOut()
{
	if (!HttpRequest.IsValid())
		return 0.0f;

	if (HttpRequest->GetTimeout().IsSet())
		return HttpRequest->GetTimeout().GetValue();

	return 0.0f;
}

void UHTTPObject::ClearTimeOut()
{
	if (!HttpRequest.IsValid())
		return;
	HttpRequest->ClearTimeout();
}

void UHTTPObject::Tick(float DeltaSeconds)
{
	if (!HttpRequest.IsValid())
		return;
	HttpRequest->Tick(DeltaSeconds);
}

void UHTTPObject::ProcessRequest(TEnumAsByte<EOutputExecPins>& Output)
{
	if (!HttpRequest.IsValid())
	{
		Output = EOutputExecPins::Failure;
		return;
	}
		
	if (HttpRequest->ProcessRequest())
	{
		Output = EOutputExecPins::Success;
		return;
	}
	
	Output = EOutputExecPins::Failure;
}

void UHTTPObject::CancelRequest()
{
	if (!HttpRequest.IsValid())
		return;
	HttpRequest->CancelRequest();
}
