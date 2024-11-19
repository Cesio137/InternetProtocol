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
	virtual void BeginDestroy() override
	{
		ShouldStopContext = true;
		TCP.resolver.cancel();
		if (IsConnected()) Close();
		ThreadPool->wait();
		consume_response_buffer();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Remote")
	void SetHost(const FString& ip, const FString& port)
	{
		Host = ip;
		Service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Local")
	FString GetLocalAdress() const
	{
		if (IsConnected())
			return TCP.socket.local_endpoint().address().to_string().c_str();
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Local")
	FString GetLocalPort() const
	{
		if (IsConnected())
			return FString::FromInt(TCP.socket.local_endpoint().port());
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Remote")
	FString GetRemoteAdress() const
	{
		if (IsConnected())
			return TCP.socket.remote_endpoint().address().to_string().c_str();
		return Host;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Remote")
	FString GetRemotePort() const
	{
		if (IsConnected())
			return FString::FromInt(TCP.socket.remote_endpoint().port());
		return Service;
	}

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void SetTimeout(uint8 value = 3) { Timeout = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	uint8 GetTimeout() const { return Timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void SetMaxAttemp(uint8 value = 3) { MaxAttemp = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	uint8 GetMaxAttemp() const { return Timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void SetMaxSendBufferSize(int value = 1400) { MaxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void SetSplitPackage(bool value = true) { bSplitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	bool GetSplitPackage() const { return bSplitBuffer; }

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool Send(const FString& message);
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool SendRaw(const TArray<uint8>& buffer);
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool AsyncRead();

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	bool Connect();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Connection")
	bool IsConnected() const { return TCP.socket.is_open(); }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	void Close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Error")
	int GetErrorCode() const { return TCP.error_code.value(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Error")
	FString GetErrorMessage() const { return TCP.error_code.message().data(); }

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
	TUniquePtr<asio::thread_pool> ThreadPool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	bool ShouldStopContext = false;
	FAsioTcp TCP;
	FString Host = "localhost";
	FString Service;
	uint8_t Timeout = 3;
	uint8_t MaxAttemp = 3;
	bool bSplitBuffer = true;
	int MaxSendBufferSize = 1400;
	asio::streambuf ResponseBuffer;

	void package_string(const FString& str);
	void package_buffer(const TArray<uint8>& buffer);

	void consume_response_buffer()
	{
		ResponseBuffer.consume(ResponseBuffer.size());
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
	virtual void BeginDestroy() override
	{
		ShouldStopContext = true;
		TCP.resolver.cancel();
		if (IsConnected()) Close();
		ThreadPool->wait();
		consume_response_buffer();
		Super::BeginDestroy();
	}

	/*HOST*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Remote")
	void SetHost(const FString& ip, const FString& port)
	{
		Host = ip;
		Service = port;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Local")
	FString GetLocalAdress() const
	{
		if (IsConnected())
			return TCP.ssl_socket.lowest_layer().local_endpoint().address().to_string().c_str();
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Local")
	FString GetLocalPort() const
	{
		if (IsConnected())
			return FString::FromInt(TCP.ssl_socket.lowest_layer().local_endpoint().port());
		return "";
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Remote")
	FString GetRemoteAdress() const
	{
		if (IsConnected())
			return TCP.ssl_socket.lowest_layer().remote_endpoint().address().to_string().c_str();
		return Host;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Remote")
	FString GetRemotePort() const
	{
		if (IsConnected())
			return FString::FromInt(TCP.ssl_socket.lowest_layer().remote_endpoint().port());
		return Service;
	}

	/*SETTINGS*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void SetTimeout(uint8 value = 3) { Timeout = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	uint8 GetTimeout() const { return Timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void SetMaxAttemp(uint8 value = 3) { MaxAttemp = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	uint8 GetMaxAttemp() const { return Timeout; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void SetMaxSendBufferSize(int value = 1400) { MaxSendBufferSize = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	int GetMaxSendBufferSize() const { return MaxSendBufferSize; }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Settings")
	void SetSplitPackage(bool value = true) { SplitBuffer = value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Settings")
	bool GetSplitPackage() const { return SplitBuffer; }

	/*SECURITY LAYER*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
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

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Security Layer")
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

	/*MESSAGE*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool Send(const FString& message);
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool SendRaw(const TArray<uint8>& buffer);
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Message")
	bool AsyncRead();

	/*CONNECTION*/
	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	bool Connect();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Connection")
	bool IsConnected() const { return TCP.ssl_socket.lowest_layer().is_open(); }

	UFUNCTION(BlueprintCallable, Category = "IP||TCP||Connection")
	void Close();

	/*ERRORS*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Error")
	int GetErrorCode() const { return TCP.error_code.value(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP||TCP||Error")
	FString GetErrorMessage() const { return TCP.error_code.message().data(); }

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
	TUniquePtr<asio::thread_pool> ThreadPool = MakeUnique<asio::thread_pool>(std::thread::hardware_concurrency());
	FCriticalSection MutexIO;
	FCriticalSection MutexBuffer;
	bool ShouldStopContext = false;
	FAsioTcpSsl TCP;
	FString Host = "localhost";
	FString Service;
	uint8_t Timeout = 3;
	uint8_t MaxAttemp = 3;
	bool SplitBuffer = true;
	int MaxSendBufferSize = 1400;
	asio::streambuf ResponseBuffer;

	void package_string(const FString& str);
	void package_buffer(const TArray<uint8>& buffer);

	void consume_response_buffer()
	{
		ResponseBuffer.consume(ResponseBuffer.size());
	}

	void run_context_thread();
	void resolve(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints);
	void conn(const asio::error_code& error);
	void ssl_handshake(const asio::error_code& error);
	void write(const asio::error_code& error, const size_t bytes_sent);
	void read(const asio::error_code& error, const size_t bytes_recvd);
};
