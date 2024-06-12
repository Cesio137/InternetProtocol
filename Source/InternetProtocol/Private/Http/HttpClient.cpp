// Fill out your copyright notice in the Description page of Project Settings.

#include "Http/HttpClient.h"
#include <iostream>

void UHttpClient::preparePayload()
{
	payload.Empty();
	
	payload = verb[request.verb] + " " + request.path;
	
	if (request.params.Num() > 0)
	{
		payload += "?";
		bool first = true;
		for (const TPair<FString, FString>& param : request.params) {
			if (!first) payload += "&";
			payload += param.Key + "=" + param.Value;
			first = false;
		}
	}
	payload += " HTTP/" + request.version + "\r\n";

	payload += "Host: " + host;
	if (!service.IsEmpty()) payload += ":" + service;
	payload += "\r\n";

	if (request.headers.Num() > 0)
	{
		for (const TPair<FString, FString>& header : request.headers) {
			payload += header.Key + ": " + header.Value + "\r\n";
		}
		payload += "Content-Length: " + FString::FromInt(request.body.Len()) + "\r\n";
	}
	payload += "\r\n";

	if (!request.body.IsEmpty())
		payload += "\r\n" + request.body;
}

int UHttpClient::async_preparePayload()
{
	if (!pool.IsValid())
		return -1;
	
	asio::post(*pool, [this]()
	{
		mutexPayload.lock();
		preparePayload();
		OnAsyncPayloadFinished.Broadcast();
		mutexPayload.unlock();
	});
	
	return 0;
}

int UHttpClient::processRequest()
{
	if (!pool.IsValid())
		return -1;
	
	asio::post(*pool, [this]()
	{
		runContextThread();
	});
	
	return 0;
}

void UHttpClient::runContextThread()
{
	mutexIO.lock();
	asio.resolver.async_resolve(TCHAR_TO_UTF8(*getHost()),
		TCHAR_TO_UTF8(*getPort()),
		std::bind(&UHttpClient::resolve, this, asio::placeholders::error, asio::placeholders::results)
	);
	if (asio.context.stopped())
		asio.context.restart();
	asio.context.run();
	asio.context.restart();
	if (getErrorCode() != 0 && maxretry > 0 && retrytime > 0) {
		int i = 0;
		while (getErrorCode() != 0 && i < maxretry) {
			i++;
			OnRequestWillRetry.Broadcast(i, retrytime);
			std::this_thread::sleep_for(std::chrono::seconds(retrytime));
			asio.resolver.async_resolve(TCHAR_TO_UTF8(*getHost()),
				TCHAR_TO_UTF8(*getPort()),
				std::bind(&UHttpClient::resolve, this, asio::placeholders::error, asio::placeholders::results)
			);
			asio.context.run();
			asio.context.restart();
		}
	}
	OnRequestCompleted.Broadcast(response);
	mutexIO.unlock();
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
	OnConnected.Broadcast();
	std::ostream request_stream(&request_buffer);
	request_stream << TCHAR_TO_UTF8(*payload);
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
		OnRequestError.Broadcast(status_code, "Invalid response.");
		return;
	}
	if (status_code != 200)
	{
		//std::cout << "Response returned with status code ";
		//std::cout << status_code << "\n";
		OnRequestError.Broadcast(status_code, "Response returned with status code.");
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
	response.clear();
	std::istream response_stream(&response_buffer);
	std::string header;
	
	while (std::getline(response_stream, header) && header != "\r")
		response.appendHeader(header.data());

	// Write whatever content we already have to output.
	std::ostringstream content_buffer;
	if (response_buffer.size() > 0)
	{
		content_buffer << &response_buffer;
		response.setContent(content_buffer.str().data());
	}
	
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
		OnRequestCompleted.Broadcast(response);
		return;
	}
	// Write all of the data that has been read so far.
	std::ostringstream stream_buffer;
	stream_buffer << &response_buffer;
	std::cout << "stream_Buffer: " << stream_buffer.str() << std::endl;
	if (!stream_buffer.str().empty())
		response.appendContent(stream_buffer.str().data());
	
	// Continue reading remaining data until EOF.
	asio::async_read(asio.socket, 
		response_buffer,
		asio::transfer_at_least(1),
		std::bind(&UHttpClient::read_content, this, asio::placeholders::error)
	);
}
