//© Nathan Miguel, 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "IWebSocket.h"
#include "WebSocketsModule.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "Library/StructLibrary.h"
#include "Websocket.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateOnConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateOnMessage, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateOnRawMessage, const FVoid&, Data, int, Size, int, BytesRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateOnConnectionError, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDelegateOnClosed, int32, StatusCode, const FString&, Reason, bool, bWasClean);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateOnMessageSent, const FString&, MessageString);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UWebsocket : public UObject
{
	GENERATED_BODY()
protected:
	virtual void BeginDestroy() override;
	
public:
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket", meta=(ExpandEnumAsExecs = "Output"))
	void ConstructWebsocket(TEnumAsByte<EOutputExecPins>& Output, const FString& url, const FString& protocol = "ws");

	UFUNCTION(BlueprintCallable, Category="IP||Websocket")
	void Connect();

	UFUNCTION(BlueprintCallable, Category="IP||Websocket")
	void Close(int32 code = 1000, const FString& reason = "");

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="IP||Websocket")
	bool bIsConnected();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="IP||Websocket")
	bool bIsWebsocketValid();

	UFUNCTION(BlueprintCallable, Category="IP||Websocket")
	void Reset();

	//Send
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket")
	void Send(const FString& Data);

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket")
	void SendRaw(const TArray<uint8>& Data);
	
	//Events
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateOnConnected OnConnected;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateOnMessage OnMessage;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateOnConnectionError OnConnectionError;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateOnClosed OnClosed;
	
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateOnMessageSent OnMessageSent;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||Websocket||Events")
	FDelegateOnRawMessage OnRawMessage;

private:
	TSharedPtr<IWebSocket> websocket;
};
