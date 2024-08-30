// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/Net/Message.h"
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
	UWebsocketClient()
	{
	}

	virtual ~UWebsocketClient() override
	{
		tcp.context.stop();
		pool->stop();
		consume_response_buffer();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Remote")
	void setHost(const FString& url = "localhost", const FString& port = "")
	{
		host = url;
		service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Local")
	FString getLocalAdress() const
	{
		if (isConnected())
		{
			return UTF8_TO_TCHAR(tcp.socket.local_endpoint().address().to_string().c_str());
		}
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Local")
	FString getLocalPort() const
	{
		if (isConnected())
		{
			return FString::FromInt(tcp.socket.local_endpoint().port());
		}
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Remote")
	FString getRemoteAdress() const
	{
		if (isConnected())
		{
			return UTF8_TO_TCHAR(tcp.socket.remote_endpoint().address().to_string().c_str());
		}
		return host;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Remote")
	FString getRemovePort() const
	{
		if (isConnected())
		{
			return FString::FromInt(tcp.socket.remote_endpoint().port());
		}
		return service;
	}

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Settings")
	void setTimeout(uint8 value = 3) { timeout = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Settings")
	uint8 getTimeout() const { return timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Settings")
	void setMaxAttemp(uint8 value = 3) { maxAttemp = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Settings")
	uint8 getMaxAttemp() const { return timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Settings")
	void setMaxSendBufferSize(int value = 1400) { maxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Settings")
	int getMaxSendBufferSize() const { return maxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Settings")
	void setSplitPackage(bool value = true) { splitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Settings")
	bool getSplitPackage() const { return splitBuffer; }


	/*HANDSHAKE*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void setPath(const FString& value = "chat") { handshake.path = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString getPath() const { return handshake.path; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void setVersion(const FString& value = "1.1") { handshake.version = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString getVersion() const { return handshake.version; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void setKey(const FString& value = "dGhlIHNhbXBsZSBub25jZQ==") { handshake.Sec_WebSocket_Key = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString getKey() const { return handshake.Sec_WebSocket_Key; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void setOrigin(const FString& value = "client") { handshake.origin = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString getOrigin() const { return handshake.origin; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void setSecProtocol(const FString& value = "chat, superchat") { handshake.Sec_WebSocket_Protocol = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString getSecProtocol() const { return handshake.Sec_WebSocket_Protocol; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Handshake")
	void setSecVersion(const FString& value = "13") { handshake.Sec_Websocket_Version = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Handshake")
	FString getSecVersion() const { return handshake.Sec_Websocket_Version; }

	/*DATAFRAME*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Dataframe")
	void setRSV1(bool value = false) { sDataFrame.rsv1 = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Dataframe")
	bool useRSV1() const { return sDataFrame.rsv1; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Dataframe")
	void setRSV2(bool value = false) { sDataFrame.rsv2 = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Dataframe")
	bool useRSV2() const { return sDataFrame.rsv2; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Dataframe")
	void setRSV3(bool value = false) { sDataFrame.rsv3 = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Dataframe")
	bool useRSV3() const { return sDataFrame.rsv3; }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Dataframe")
	void setMask(bool value = true) { sDataFrame.mask = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Dataframe")
	bool useMask() const { return sDataFrame.mask; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Message")
	bool send(const FString& message);
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Message")
	bool sendRaw(const TArray<uint8> buffer);
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Message")
	bool sendPing();
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Message")
	bool asyncRead();

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Connection")
	bool connect();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||Websocket||Connection")
	bool isConnected() const { return tcp.socket.is_open(); }

	UFUNCTION(BlueprintCallable, Category = "IP||Websocket||Connection")
	void close();

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
	TUniquePtr<asio::thread_pool> pool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	std::mutex mutexIO;
	std::mutex mutexBuffer;
	FString host = "localhost";
	FString service;
	uint8 timeout = 3;
	uint8 maxAttemp = 3;
	bool splitBuffer = false;
	int maxSendBufferSize = 1400;
	FAsioTcp tcp;
	asio::streambuf request_buffer;
	asio::streambuf response_buffer;
	FHandShake handshake;
	FDataFrame sDataFrame;

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
		request_buffer.consume(request_buffer.size());
		response_buffer.consume(response_buffer.size());
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