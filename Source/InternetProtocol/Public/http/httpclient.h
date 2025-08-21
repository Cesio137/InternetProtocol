/** 
 * Copyright © 2025 Nathan Miguel
 */

#pragma once

#include "CoreMinimal.h"
#include "net/asio.h"
#include "httpclient.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_DELEGATE_TwoParams(FDelegateHttpClientResponse, const FErrorCode &, ErrorCode, const FHttpResponse&, Response);

struct http_client_t {
	http_client_t(): socket(context), resolver(context) {
	}

	asio::io_context context;
	tcp::socket socket;
	tcp::endpoint endpoint;
	tcp::resolver resolver;
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|HTTP")
class INTERNETPROTOCOL_API UHttpClient : public UObject
{
	GENERATED_BODY()
public:
	UHttpClient() {
		idle_timer = MakeUnique<asio::steady_timer>(net.context);
	}
	~UHttpClient() {
		if (net.socket.is_open())
			Close();
		consume_recv_buffer();
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "IP|HTTP")
	uint8 IdleTimeoutSeconds = 0;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP")
	bool IsOpen();

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP")
	void SetHost(const FClientBindOptions& BindOpts);

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP")
	void Request(const FHttpRequest& Request, const FDelegateHttpClientResponse& Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP")
	void Close();

private:
	FCriticalSection mutex_io;
	TAtomic<bool> is_closing = false;
	TUniquePtr<asio::steady_timer> idle_timer;
	FClientBindOptions bind_options;
	http_client_t net;
	asio::streambuf recv_buffer;

	void start_idle_timer();
	void reset_idle_timer();
	void run_context_thread();
	void resolve(const asio::error_code &error, const tcp::resolver::results_type &results,
					 const FHttpRequest &req,
					 const FDelegateHttpClientResponse& response_cb);
	void conn(const asio::error_code &error, const FHttpRequest &req,
				  const FDelegateHttpClientResponse& response_cb);
	void write_cb(const asio::error_code &error, const size_t bytes_sent,
					  const FDelegateHttpClientResponse& response_cb);
	void consume_recv_buffer();
	void read_cb(const asio::error_code &error, const size_t bytes_recvd,
					 const FDelegateHttpClientResponse& response_cb);
	void read_headers(const asio::error_code &error, FHttpResponse &response,
						  const FDelegateHttpClientResponse& response_cb);
};

struct http_client_ssl_t {
	http_client_ssl_t(): ssl_context(asio::ssl::context::tlsv13_client),
						ssl_socket(context, ssl_context),
						resolver(context) {
	}

	asio::io_context context;
	asio::ssl::context ssl_context;
	tcp::resolver resolver;
	tcp::endpoint endpoint;
	asio::ssl::stream<tcp::socket> ssl_socket;
};

UCLASS(Blueprintable, BlueprintType, Category = "IP|HTTP")
class INTERNETPROTOCOL_API UHttpClientSsl : public UObject
{
	GENERATED_BODY()
public:
	UHttpClientSsl() {
		idle_timer = MakeUnique<asio::steady_timer>(net.context);
	}
	~UHttpClientSsl() {
		if (net.ssl_socket.next_layer().is_open())
			Close();
		consume_recv_buffer();
	}

	void Construct(const FSecurityContextOpts &SecOpts) {
		file_format_e file_format = SecOpts.File_Format == EFileFormat::asn1 ? file_format_e::asn1 : file_format_e::pem;
		if (!SecOpts.Private_Key.IsEmpty()) {
			const asio::const_buffer buffer(SecOpts.Private_Key.GetCharArray().GetData(), SecOpts.Private_Key.Len());
			net.ssl_context.use_private_key(buffer, file_format);
		}

		if (!SecOpts.Cert.IsEmpty()) {
			const asio::const_buffer buffer(SecOpts.Cert.GetCharArray().GetData(), SecOpts.Cert.Len());
			net.ssl_context.use_certificate(buffer, file_format);
		}

		if (!SecOpts.Cert_Chain.IsEmpty()) {
			const asio::const_buffer buffer(SecOpts.Cert_Chain.GetCharArray().GetData(), SecOpts.Cert_Chain.Len());
			net.ssl_context.use_certificate_chain(buffer);
		}

		if (!SecOpts.RSA_Private_Key.IsEmpty()) {
			const asio::const_buffer buffer(SecOpts.RSA_Private_Key.GetCharArray().GetData(), SecOpts.RSA_Private_Key.Len());
			net.ssl_context.use_rsa_private_key(buffer, file_format);
		}

		if (!SecOpts.Host_Name_Verification.IsEmpty()) {
			net.ssl_context.set_verify_callback(asio::ssl::host_name_verification(TCHAR_TO_UTF8(*SecOpts.Host_Name_Verification)));
		}

		switch (SecOpts.Verify_Mode) {
		case EVerifyMode::None:
			net.ssl_context.set_verify_mode(asio::ssl::verify_none);
			break;
		case EVerifyMode::Verify_Peer:
			net.ssl_context.set_verify_mode(asio::ssl::verify_peer);
			break;
		case EVerifyMode::Verify_Fail_If_No_Peer_Cert:
			net.ssl_context.set_verify_mode(asio::ssl::verify_fail_if_no_peer_cert);
			break;
		case EVerifyMode::Verify_Client_Once:
			net.ssl_context.set_verify_mode(asio::ssl::verify_client_once);
			break;
		default:
			break;
		}

		net.ssl_socket = asio::ssl::stream<tcp::socket>(net.context, net.ssl_context);
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "IP|HTTP")
	uint8 IdleTimeoutSeconds = 0;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IP|HTTP")
	bool IsOpen();

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP")
	void SetHost(const FClientBindOptions& BindOpts);

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP")
	void Request(const FHttpRequest& Request, const FDelegateHttpClientResponse& Callback);

	UFUNCTION(BlueprintCallable, Category = "IP|HTTP")
	void Close();

private:
	FCriticalSection mutex_io;
	TAtomic<bool> is_closing = false;
	TUniquePtr<asio::steady_timer> idle_timer;
	FClientBindOptions bind_options;
	http_client_ssl_t net;
	asio::streambuf recv_buffer;

	void start_idle_timer();
	void reset_idle_timer();
	void run_context_thread();
	void resolve(const asio::error_code &error, const tcp::resolver::results_type &results,
					 const FHttpRequest &req,
					 const FDelegateHttpClientResponse& response_cb);
	void conn(const asio::error_code &error, const FHttpRequest &req,
				  const FDelegateHttpClientResponse& response_cb);
	void ssl_handshake(const asio::error_code &error,
						   const FHttpRequest &req,
						   const FDelegateHttpClientResponse& response_cb);
	void write_cb(const asio::error_code &error, const size_t bytes_sent,
					  const FDelegateHttpClientResponse& response_cb);
	void consume_recv_buffer();
	void read_cb(const asio::error_code &error, const size_t bytes_recvd,
					 const FDelegateHttpClientResponse& response_cb);
	void read_headers(const asio::error_code &error, FHttpResponse &response,
						  const FDelegateHttpClientResponse& response_cb);
};
