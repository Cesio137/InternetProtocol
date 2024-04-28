//© Nathan Miguel, 2024. All Rights Reserved.


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
	TimeoutSecs = 3.0;
}

void UHTTPObject::SetUrl(const FString& URL)
{
	if (!HttpRequest.IsValid())
		return;

	if (URL.IsEmpty())
		return;

	Url = URL;
	HttpRequest->SetURL(URL);
}

const FString UHTTPObject::GetUrl()
{
	return Url;
}

FString UHTTPObject::EncodeUrl(const FString& url)
{
	if (url.IsEmpty())
		return FString();

	FString EncodedURL = url;
	EncodedURL.ReplaceInline(TEXT(" "), TEXT("%20"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("!"), TEXT("%21"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("\""), TEXT("%22"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("#"), TEXT("%23"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("$"), TEXT("%24"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("&"), TEXT("%26"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("'"), TEXT("%27"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("("), TEXT("%28"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT(")"), TEXT("%29"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("*"), TEXT("%2A"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("+"), TEXT("%2B"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT(","), TEXT("%2C"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("-"), TEXT("%2D"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("."), TEXT("%2E"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("/"), TEXT("%2F"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT(":"), TEXT("%3A"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT(";"), TEXT("%3B"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("<"), TEXT("%3C"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("="), TEXT("%3D"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT(">"), TEXT("%3E"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("?"), TEXT("%3F"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("@"), TEXT("%40"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("["), TEXT("%5B"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("\\"), TEXT("%5C"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("]"), TEXT("%5D"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("^"), TEXT("%5E"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("_"), TEXT("%5F"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("`"), TEXT("%60"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("{"), TEXT("%7B"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("|"), TEXT("%7C"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("}"), TEXT("%7D"), ESearchCase::CaseSensitive);
	EncodedURL.ReplaceInline(TEXT("~"), TEXT("%7E"), ESearchCase::CaseSensitive);

	return EncodedURL;
}

FString UHTTPObject::DecodeUrl(const FString& url)
{
	if (url.IsEmpty())
		return FString();

	FString DecodedURL = url;
	DecodedURL.ReplaceInline(TEXT(" "), TEXT("%20"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("!"), TEXT("%21"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("\""), TEXT("%22"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("#"), TEXT("%23"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("$"), TEXT("%24"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("&"), TEXT("%26"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("'"), TEXT("%27"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("("), TEXT("%28"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT(")"), TEXT("%29"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("*"), TEXT("%2A"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("+"), TEXT("%2B"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT(","), TEXT("%2C"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("-"), TEXT("%2D"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("."), TEXT("%2E"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("/"), TEXT("%2F"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT(":"), TEXT("%3A"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT(";"), TEXT("%3B"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("<"), TEXT("%3C"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("="), TEXT("%3D"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT(">"), TEXT("%3E"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("?"), TEXT("%3F"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("@"), TEXT("%40"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("["), TEXT("%5B"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("\\"), TEXT("%5C"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("]"), TEXT("%5D"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("^"), TEXT("%5E"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("_"), TEXT("%5F"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("`"), TEXT("%60"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("{"), TEXT("%7B"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("|"), TEXT("%7C"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("}"), TEXT("%7D"), ESearchCase::CaseSensitive);
	DecodedURL.ReplaceInline(TEXT("~"), TEXT("%7E"), ESearchCase::CaseSensitive);

	return DecodedURL;
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
