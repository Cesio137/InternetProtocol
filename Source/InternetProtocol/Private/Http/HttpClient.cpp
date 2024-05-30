// Fill out your copyright notice in the Description page of Project Settings.

#include "Http/HttpClient.h"
#include <iostream>

FString UHttpClient::getData()
{
	FString request_data;

	request_data = verb[request.verb] + " " + request.path;
	
	if (request.params.Num() < 1)
	{
		request_data += "?";
		bool first = true;
		for (const TPair<FString, FString>& param : request.params) {
			if (!first) request_data += "&";
			request_data += param.Key + "=" + param.Value;
			first = false;
		}
	}
	request_data += " HTTP/" + request.version + "\r\n";

	request_data += "Host: " + host;
	if (!service.IsEmpty()) request_data += ":" + service;
	request_data += "\r\n";

	if (request.headers.Num() < 1)
	{
		for (const TPair<FString, FString>& header : request.headers) {
			request_data += header.Key + ": " + header.Value + "\r\n";
		}
		request_data += "Content-Length: " + FString::FromInt(request.body.Len()) + "\r\n";
	}
	request_data += "\r\n";

	if (!request.body.IsEmpty())
		request_data += "\r\n" + request.body;

	return request_data;
}

int UHttpClient::processRequest()
{
	std::ostream request_stream(&request_buffer);
	request_stream << GetData(getData());

	asio.resolver.async_resolve(TCHAR_TO_UTF8(*getHost()),
		TCHAR_TO_UTF8(*getPort()),
		std::bind(&UHttpClient::resolve, this, asio::placeholders::error, asio::placeholders::results)
	);
	asio.context.run();
	return 0;
}

void UHttpClient::resolve(const std::error_code& err, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (err)
	{
		OnRequestError.Broadcast(err.value(), err.message().data());
		return;
	}
	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.
		
	asio::async_connect(asio.socket,
		endpoints,
		std::bind(&UHttpClient::connect, this, asio::placeholders::error)
	);
}

void UHttpClient::connect(const std::error_code& err)
{
	if (err)
	{
		asio.error_code = err;
		OnRequestError.Broadcast(err.value(), err.message().data());
		return;
	}
	// The connection was successful. Send the request.
	asio::async_write(asio.socket,
		request_buffer,
		std::bind(&UHttpClient::write_request, this, asio::placeholders::error)
	);
}

void UHttpClient::write_request(const std::error_code& err)
{
	if (err)
	{
		asio.error_code = err;
		OnRequestError.Broadcast(err.value(), err.message().data());
		return;
	}
	// Read the response status line. The response_ streambuf will
	// automatically grow to accommodate the entire line. The growth may be
	// limited by passing a maximum size to the streambuf constructor.
	asio::async_read_until(asio.socket, 
		response_buffer, "\r\n",
		std::bind(&UHttpClient::read_status_line, this, asio::placeholders::error)
	);
}

void UHttpClient::read_status_line(const std::error_code& err)
{
	if (err)
	{
		asio.error_code = err;
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
	asio::async_read_until(asio.socket, 
		response_buffer, "\r\n\r\n",
		std::bind(&UHttpClient::read_headers, this, asio::placeholders::error)
	);
}

void UHttpClient::read_headers(const std::error_code& err)
{
	if (err)
	{
		asio.error_code = err;
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
	asio::async_read(asio.socket,
		response_buffer,
		asio::transfer_at_least(1),
		std::bind(&UHttpClient::read_content, this, asio::placeholders::error)
	);
}

void UHttpClient::read_content(const std::error_code& err)
{
	if (err)
	{
		asio.error_code = err;
		OnRequestError.Broadcast(err.value(), err.message().data());
		return;
	}
	// Write all of the data that has been read so far.
	//std::cout << &response_buffer;

	// Continue reading remaining data until EOF.
	asio::async_read(asio.socket, 
		response_buffer,
		asio::transfer_at_least(1),
		std::bind(&UHttpClient::read_content, this, asio::placeholders::error)
	);
}
