// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Net/Commons.h"
#include "Net/Message.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "TCPClient.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDelegateTcpConnection);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpRetry, const int, attemp);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelegateTcpMessageSent, const int, size);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpMessageReceived, const int, size, const FTcpMessage, message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDelegateTcpError, const int, code, const FString&, exception);

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UTCPClient : public UObject
{
	GENERATED_BODY()

public:
	UTCPClient()
	{
	}

	virtual void BeginDestroy() override
	{
		ShouldStopContext = true;
		tcp.resolver.cancel();
		if (isConnected()) close();
		consume_response_buffer();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Remote")
	void setHost(const FString& ip, const FString& port)
	{
		host = ip;
		service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Local")
	FString getLocalAdress() const
	{
		if (isConnected())
			return tcp.socket.local_endpoint().address().to_string().c_str();
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Local")
	FString getLocalPort() const
	{
		if (isConnected())
			return FString::FromInt(tcp.socket.local_endpoint().port());
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Remote")
	FString getRemoteAdress() const
	{
		if (isConnected())
			return tcp.socket.remote_endpoint().address().to_string().c_str();
		return host;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Remote")
	FString getRemotePort() const
	{
		if (isConnected())
			return FString::FromInt(tcp.socket.remote_endpoint().port());
		return service;
	}

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setTimeout(uint8 value = 3) { timeout = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	uint8 getTimeout() const { return timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setMaxAttemp(uint8 value = 3) { maxAttemp = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	uint8 getMaxAttemp() const { return timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setMaxSendBufferSize(int value = 1400) { maxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	int getMaxSendBufferSize() const { return maxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setSplitPackage(bool value = true) { splitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	bool getSplitPackage() const { return splitBuffer; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool send(const FString& message);
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool sendRaw(const TArray<uint8>& buffer);
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool asyncRead();

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	bool connect();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Connection")
	bool isConnected() const { return tcp.socket.is_open(); }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	void close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Error")
	int getErrorCode() const { return tcp.error_code.value(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Error")
	FString getErrorMessage() const { return tcp.error_code.message().data(); }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpConnection OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpRetry OnConnectionWillRetry;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpError OnError;

private:
	std::mutex mutexIO;
	std::mutex mutexBuffer;
	bool ShouldStopContext = false;
	FAsioTcp tcp;
	FString host = "localhost";
	FString service;
	uint8_t timeout = 3;
	uint8_t maxAttemp = 3;
	bool splitBuffer = true;
	int maxSendBufferSize = 1400;
	asio::streambuf response_buffer;

	void package_string(const FString& str);
	void package_buffer(const TArray<uint8>& buffer);

	void consume_response_buffer()
	{
		response_buffer.consume(response_buffer.size());
	}

	void run_context_thread();
	void resolve(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void conn(const asio::error_code& error);
	void write(const asio::error_code& error, const size_t bytes_sent);
	void read(const asio::error_code& error, const size_t bytes_recvd);
};

UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UTCPClientSsl : public UObject
{
	GENERATED_BODY()

public:
	UTCPClientSsl()
	{
	}
	
	virtual void BeginDestroy() override
	{
		ShouldStopContext = true;
		tcp.resolver.cancel();
		if (isConnected()) close();
		consume_response_buffer();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Remote")
	void setHost(const FString& ip, const FString& port)
	{
		host = ip;
		service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Local")
	FString getLocalAdress() const
	{
		if (isConnected())
			return tcp.ssl_socket.lowest_layer().local_endpoint().address().to_string().c_str();
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Local")
	FString getLocalPort() const
	{
		if (isConnected())
			return FString::FromInt(tcp.ssl_socket.lowest_layer().local_endpoint().port());
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Remote")
	FString getRemoteAdress() const
	{
		if (isConnected())
			return tcp.ssl_socket.lowest_layer().remote_endpoint().address().to_string().c_str();
		return host;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Remote")
	FString getRemotePort() const
	{
		if (isConnected())
			return FString::FromInt(tcp.ssl_socket.lowest_layer().remote_endpoint().port());
		return service;
	}

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setTimeout(uint8 value = 3) { timeout = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	uint8 getTimeout() const { return timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setMaxAttemp(uint8 value = 3) { maxAttemp = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	uint8 getMaxAttemp() const { return timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setMaxSendBufferSize(int value = 1400) { maxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	int getMaxSendBufferSize() const { return maxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void setSplitPackage(bool value = true) { splitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	bool getSplitPackage() const { return splitBuffer; }

	/*SECURITY LAYER*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool load_private_key_data(const FString& key_data) noexcept
	{
		if (key_data.IsEmpty()) return false;
		asio::error_code ec;
		std::string key = TCHAR_TO_UTF8(*key_data);
		const asio::const_buffer buffer(key.data(), key.size());
		tcp.ssl_context.use_private_key(buffer, asio::ssl::context::pem, ec);
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool load_private_key_file(const FString& filename)
	{
		if (filename.IsEmpty()) return false;
		asio::error_code ec;
		std::string file = TCHAR_TO_UTF8(*filename);
		tcp.ssl_context.use_private_key_file(file, asio::ssl::context::pem, ec);
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool load_certificate_data(const FString& cert_data)
	{
		if (cert_data.IsEmpty()) return false;
		asio::error_code ec;
		std::string cert = TCHAR_TO_UTF8(*cert_data);
		const asio::const_buffer buffer(cert.data(), cert.size());
		tcp.ssl_context.use_certificate(buffer, asio::ssl::context::pem);
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool load_certificate_file(const FString& filename)
	{
		if (filename.IsEmpty()) return false;
		asio::error_code ec;
		std::string file = TCHAR_TO_UTF8(*filename);
		tcp.ssl_context.use_certificate_file(file, asio::ssl::context::pem, ec);
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool load_certificate_chain_data(const FString& cert_chain_data)
	{
		if (cert_chain_data.IsEmpty()) return false;
		asio::error_code ec;
		std::string cert_chain = TCHAR_TO_UTF8(*cert_chain_data);
		const asio::const_buffer buffer(cert_chain.data(),
										cert_chain.size());
		tcp.ssl_context.use_certificate_chain(buffer, ec);
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool load_certificate_chain_file(const FString& filename)
	{
		if (filename.IsEmpty()) return false;
		asio::error_code ec;
		std::string file = TCHAR_TO_UTF8(*filename);
		tcp.ssl_context.use_certificate_chain_file(file, ec);
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
	bool load_verify_file(const FString& filename)
	{
		if (filename.IsEmpty()) return false;
		asio::error_code ec;
		std::string file = TCHAR_TO_UTF8(*filename);
		tcp.ssl_context.load_verify_file(file, ec);
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

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool send(const FString& message);
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool sendRaw(const TArray<uint8>& buffer);
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool asyncRead();

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	bool connect();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Connection")
	bool isConnected() const { return tcp.ssl_socket.lowest_layer().is_open(); }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	void close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Error")
	int getErrorCode() const { return tcp.error_code.value(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Error")
	FString getErrorMessage() const { return tcp.error_code.message().data(); }

	/*EVENTS*/
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpConnection OnConnected;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpRetry OnConnectionWillRetry;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpConnection OnClose;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpMessageSent OnMessageSent;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||TCP||Events")
	FDelegateTcpError OnError;

private:
	std::mutex mutexIO;
	std::mutex mutexBuffer;
	bool ShouldStopContext = false;
	FAsioTcpSsl tcp;
	FString host = "localhost";
	FString service;
	uint8_t timeout = 3;
	uint8_t maxAttemp = 3;
	bool splitBuffer = true;
	int maxSendBufferSize = 1400;
	asio::streambuf response_buffer;

	void package_string(const FString& str);
	void package_buffer(const TArray<uint8>& buffer);

	void consume_response_buffer()
	{
		response_buffer.consume(response_buffer.size());
	}

	void run_context_thread();
	void resolve(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void conn(const asio::error_code& error);
	void ssl_handshake(const asio::error_code& error);
	void write(const asio::error_code& error, const size_t bytes_sent);
	void read(const asio::error_code& error, const size_t bytes_recvd);
};
