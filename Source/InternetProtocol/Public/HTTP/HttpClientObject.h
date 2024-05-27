// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <iostream>
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RequestObject.h"
#include "Library/StructLibrary.h"
#include "Delegates.h"
THIRD_PARTY_INCLUDES_START
#include "asio.hpp"
THIRD_PARTY_INCLUDES_END
#include "HttpClientObject.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class INTERNETPROTOCOL_API UHttpClientObject : public UObject
{
	GENERATED_BODY()
public:
	UHttpClientObject();
	~UHttpClientObject(){}

	UPROPERTY(BlueprintReadWrite, Category="IP||HTTP")
	URequestObject* request;

	UFUNCTION(BlueprintCallable, Category="IP||HTTP")
	int processRequest()
	{
		std::ostream request_stream(&request_buffer);
		request_stream << TCHAR_TO_UTF8(*request->data());

		HttpContext.resolver.async_resolve(TCHAR_TO_UTF8(*request->getHost()),
			TCHAR_TO_UTF8(*request->getPort()),
			std::bind(&UHttpClientObject::handle_resolve, this, asio::placeholders::error, asio::placeholders::results)
		);
		HttpContext.context.run();
		return 0;
	}

	UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "IP||HTTP||Events")
	FDelegateError OnRequestError;
	
private:
	FAsio HttpContext;
	asio::streambuf request_buffer;
	asio::streambuf response_buffer;

	void handle_resolve(const std::error_code& err, const asio::ip::tcp::resolver::results_type& endpoints)
	{
		if (err)
		{
			HttpContext.error_code = err;
			OnRequestError.Broadcast(err.value(), err.message().data());
			return;
		}
		// Attempt a connection to each endpoint in the list until we
		// successfully establish a connection.
		
		asio::async_connect(HttpContext.socket,
			endpoints,
			std::bind(&UHttpClientObject::handle_connect, this, asio::placeholders::error)
		);
	}

	void handle_connect(const std::error_code& err)
	{
		if (err)
		{
			HttpContext.error_code = err;
			OnRequestError.Broadcast(err.value(), err.message().data());
			return;
		}
		// The connection was successful. Send the request.
		asio::async_write(HttpContext.socket,
			request_buffer,
			std::bind(&UHttpClientObject::handle_write_request, this, asio::placeholders::error)
		);
	}

	void handle_write_request(const std::error_code& err)
	{
		if (err)
		{
			HttpContext.error_code = err;
			OnRequestError.Broadcast(err.value(), err.message().data());
			return;
		}
		// Read the response status line. The response_ streambuf will
		// automatically grow to accommodate the entire line. The growth may be
		// limited by passing a maximum size to the streambuf constructor.
		asio::async_read_until(HttpContext.socket, 
			response_buffer, "\r\n",
			std::bind(&UHttpClientObject::handle_read_status_line, this, asio::placeholders::error)
		);
	}

	void handle_read_status_line(const std::error_code& err)
	{
		if (err)
		{
			HttpContext.error_code = err;
			OnRequestError.Broadcast(err.value(), err.message().data());
			return;
		}
		// Check that response is OK.
		std::istream response_stream(&response_buffer);
		std::string http_version;
		response_stream >> http_version;
		unsigned int status_code;
		response_stream >> status_code;
		std::string status_message;
		std::getline(response_stream, status_message);
		if (!response_stream || http_version.substr(0, 5) != "HTTP/")
		{
			//std::cout << "Invalid response\n";
			return;
		}
		if (status_code != 200)
		{
			//std::cout << "Response returned with status code ";
			//std::cout << status_code << "\n";
			return;
		}

		// Read the response headers, which are terminated by a blank line.
		asio::async_read_until(HttpContext.socket, 
			response_buffer, "\r\n\r\n",
			std::bind(&UHttpClientObject::handle_read_headers, this, asio::placeholders::error)
		);
	}

	void handle_read_headers(const std::error_code& err)
	{
		if (err)
		{
			HttpContext.error_code = err;
			OnRequestError.Broadcast(err.value(), err.message().data());
			return;
		}
		// Process the response headers.
		std::istream response_stream(&response_buffer);
		std::string header;
		while (std::getline(response_stream, header) && header != "\r")
			std::cout << header << "\n";
		std::cout << "\n";

		// Write whatever content we already have to output.
		if (response_buffer.size() > 0)
			std::cout << &response_buffer;

		// Start reading remaining data until EOF.
		asio::async_read(HttpContext.socket,
			response_buffer,
			asio::transfer_at_least(1),
			std::bind(&UHttpClientObject::handle_read_content, this, asio::placeholders::error)
		);
	}

	void handle_read_content(const std::error_code& err)
	{
		if (err)
		{
			HttpContext.error_code = err;
			OnRequestError.Broadcast(err.value(), err.message().data());
			return;
		}
		// Write all of the data that has been read so far.
		//std::cout << &response_buffer;

		// Continue reading remaining data until EOF.
		asio::async_read(HttpContext.socket, 
			response_buffer,
			asio::transfer_at_least(1),
			std::bind(&UHttpClientObject::handle_read_content, this, asio::placeholders::error)
		);
	}
	
};
