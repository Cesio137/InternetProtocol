// Fill out your copyright notice in the Description page of Project Settings.

#include "Http/HttpClient.h"

void UHttpClient::preparePayload()
{
	payload.Empty();

	payload = verb[request.verb] + " " + request.path;

	if (request.params.Num() > 0)
	{
		payload += "?";
		bool first = true;
		for (const TPair<FString, FString>& param : request.params)
		{
			if (!first)
			{
				payload += "&";
			}
			payload += param.Key + "=" + param.Value;
			first = false;
		}
	}
	payload += " HTTP/" + request.version + "\r\n";

	payload += "Host: " + host;
	if (!service.IsEmpty())
	{
		payload += ":" + service;
	}
	payload += "\r\n";

	if (request.headers.Num() > 0)
	{
		for (const TPair<FString, FString>& header : request.headers)
		{
			payload += header.Key + ": " + header.Value + "\r\n";
		}
		payload += "Content-Length: " + FString::FromInt(request.body.Len()) + "\r\n";
	}
	payload += "\r\n";

	if (!request.body.IsEmpty())
	{
		payload += "\r\n" + request.body;
	}
}

bool UHttpClient::async_preparePayload()
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=]()
	{
		mutexPayload.Lock();
		preparePayload();
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnAsyncPayloadFinished.Broadcast();
		});
		mutexPayload.Unlock();
	});

	return true;
}

bool UHttpClient::processRequest()
{
	if (!payload.IsEmpty())
	{
		return false;
	}
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=]()
	{
		run_context_thread();
	});
	return true;
}

void UHttpClient::cancelRequest()
{
	finishIO = true;
	tcp.context.stop();
	tcp.socket.shutdown(asio::ip::tcp::socket::shutdown_both, tcp.error_code);
	if (tcp.error_code)
	{
		OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
	}
	tcp.socket.close(tcp.error_code);
	if (tcp.error_code)
	{
		OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
	}
	OnRequestCanceled.Broadcast();
}

void UHttpClient::run_context_thread()
{
	mutexIO.Lock();
	while (tcp.attemps_fail <= maxAttemp)
	{
		if (finishIO) return;
		if (tcp.attemps_fail > 0)
		{
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				OnRequestWillRetry.Broadcast(tcp.attemps_fail);
			});
		}
		tcp.error_code.clear();
		tcp.context.restart();
		tcp.resolver.async_resolve(TCHAR_TO_UTF8(*getHost()), TCHAR_TO_UTF8(*getPort()),
		                           std::bind(&UHttpClient::resolve, this, asio::placeholders::error,
		                                     asio::placeholders::results)
		);
		tcp.context.run();
		if (!tcp.error_code)
		{
			break;
		}
		tcp.attemps_fail++;
		FPlatformProcess::Sleep(timeout);
	}
	consume_stream_buffers();
	tcp.attemps_fail = 0;
	mutexIO.Unlock();
}

void UHttpClient::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.

	asio::async_connect(tcp.socket, endpoints,
	                    std::bind(&UHttpClient::connect, this, asio::placeholders::error)
	);
}

void UHttpClient::connect(const std::error_code& error)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// The connection was successful. Send the request.
	std::ostream request_stream(&request_buffer);
	request_stream << TCHAR_TO_UTF8(*payload);
	asio::async_write(tcp.socket, request_buffer,
	                  std::bind(&UHttpClient::write_request, this, asio::placeholders::error,
	                            asio::placeholders::bytes_transferred)
	);
}

void UHttpClient::write_request(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// Read the response status line. The response_ streambuf will
	// automatically grow to accommodate the entire line. The growth may be
	// limited by passing a maximum size to the streambuf constructor.
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnRequestProgress.Broadcast(bytes_sent, bytes_sent);
	});

	asio::async_read_until(tcp.socket, response_buffer, "\r\n",
	                       std::bind(&UHttpClient::read_status_line, this, asio::placeholders::error, bytes_sent,
	                                 asio::placeholders::bytes_transferred)
	);
}

void UHttpClient::read_status_line(const std::error_code& error, const size_t bytes_sent, const size_t bytes_recvd)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// Check that response is OK.
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnRequestProgress.Broadcast(bytes_sent, bytes_recvd);
	});

	std::istream response_stream(&response_buffer);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnResponseError.Broadcast(1);
		});
		return;
	}
	if (status_code != 200)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnResponseError.Broadcast(status_code);
		});

		return;
	}

	// Read the response headers, which are terminated by a blank line.
	asio::async_read_until(tcp.socket, response_buffer, "\r\n\r\n",
	                       std::bind(&UHttpClient::read_headers, this, asio::placeholders::error)
	);
}

void UHttpClient::read_headers(const std::error_code& error)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// Process the response headers.
	response.clear();
	std::istream response_stream(&response_buffer);
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
	{
		response.appendHeader(header.data());
	}

	// Write whatever content we already have to output.
	std::ostringstream content_buffer;
	if (response_buffer.size() > 0)
	{
		content_buffer << &response_buffer;
		response.setContent(content_buffer.str().data());
	}

	// Start reading remaining data until EOF.
	asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
	                 std::bind(&UHttpClient::read_content, this, asio::placeholders::error)
	);
}

void UHttpClient::read_content(const std::error_code& error)
{
	if (error)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestCompleted.Broadcast(response);
		});
		return;
	}
	// Write all of the data that has been read so far.
	std::ostringstream stream_buffer;
	stream_buffer << &response_buffer;
	if (!stream_buffer.str().empty())
	{
		response.appendContent(stream_buffer.str().data());
	}

	// Continue reading remaining data until EOF.
	asio::async_read(tcp.socket, response_buffer, asio::transfer_at_least(1),
	                 std::bind(&UHttpClient::read_content, this, asio::placeholders::error)
	);
}

void UHttpClientSsl::preparePayload()
{
	payload.Empty();

	payload = verb[request.verb] + " " + request.path;

	if (request.params.Num() > 0)
	{
		payload += "?";
		bool first = true;
		for (const TPair<FString, FString>& param : request.params)
		{
			if (!first)
			{
				payload += "&";
			}
			payload += param.Key + "=" + param.Value;
			first = false;
		}
	}
	payload += " HTTP/" + request.version + "\r\n";

	payload += "Host: " + host;
	if (!service.IsEmpty())
	{
		payload += ":" + service;
	}
	payload += "\r\n";

	if (request.headers.Num() > 0)
	{
		for (const TPair<FString, FString>& header : request.headers)
		{
			payload += header.Key + ": " + header.Value + "\r\n";
		}
		payload += "Content-Length: " + FString::FromInt(request.body.Len()) + "\r\n";
	}
	payload += "\r\n";

	if (!request.body.IsEmpty())
	{
		payload += "\r\n" + request.body;
	}
}

bool UHttpClientSsl::async_preparePayload()
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=]()
	{
		mutexPayload.Lock();
		preparePayload();
		OnAsyncPayloadFinished.Broadcast();
		mutexPayload.Unlock();
	});
	return true;
}

bool UHttpClientSsl::processRequest()
{
	if (!payload.IsEmpty())
	{
		return false;
	}

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=]()
	{
		run_context_thread();
	});
	return true;
}

void UHttpClientSsl::cancelRequest()
{
	finishIO = true;
	tcp.context.stop();
	tcp.ssl_socket.shutdown(tcp.error_code);
	tcp.ssl_socket.async_shutdown([&](const std::error_code& error)
	{
		if (error)
		{
			tcp.error_code = error;
			OnError.Broadcast(error.value(), error.message().c_str());
			tcp.error_code.clear();
		}
		tcp.ssl_socket.lowest_layer().shutdown(
			asio::ip::tcp::socket::shutdown_both, tcp.error_code);
		if (tcp.error_code)
		{
			OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
		}
		tcp.ssl_socket.lowest_layer().close(tcp.error_code);
		if (tcp.error_code)
		{
			OnError.Broadcast(tcp.error_code.value(), tcp.error_code.message().c_str());
		}
		OnRequestCanceled.Broadcast();
	});
}

void UHttpClientSsl::run_context_thread()
{
	mutexIO.Lock();
	while (tcp.attemps_fail <= maxAttemp)
	{
		if (finishIO) return;
		if (tcp.attemps_fail > 0)
		{
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				OnRequestWillRetry.Broadcast(tcp.attemps_fail);
			});
		}
		tcp.error_code.clear();
		tcp.context.restart();
		tcp.resolver.async_resolve(TCHAR_TO_UTF8(*getHost()), TCHAR_TO_UTF8(*getPort()),
		                           std::bind(&UHttpClientSsl::resolve, this, asio::placeholders::error,
		                                     asio::placeholders::results)
		);
		tcp.context.run();
		if (!tcp.error_code)
		{
			break;
		}
		tcp.attemps_fail++;
		FPlatformProcess::Sleep(timeout);
	}
	consume_stream_buffers();
	tcp.attemps_fail = 0;
	mutexIO.Unlock();
}

void UHttpClientSsl::resolve(const std::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.
	asio::async_connect(tcp.ssl_socket.lowest_layer(), endpoints,
	                    std::bind(&UHttpClientSsl::connect, this, asio::placeholders::error)
	);
}

void UHttpClientSsl::connect(const std::error_code& error)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// The connection was successful.
	tcp.ssl_socket.async_handshake(asio::ssl::stream_base::client,
	                               std::bind(&UHttpClientSsl::ssl_handshake,
	                                         this, asio::placeholders::error));
}

void UHttpClientSsl::ssl_handshake(const std::error_code& error)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// The connection was successful. Send the request.
	std::ostream request_stream(&request_buffer);
	request_stream << TCHAR_TO_UTF8(*payload);
	asio::async_write(tcp.ssl_socket, request_buffer,
	                  std::bind(&UHttpClientSsl::write_request, this, asio::placeholders::error,
	                            asio::placeholders::bytes_transferred)
	);
}

void UHttpClientSsl::write_request(const std::error_code& error, const size_t bytes_sent)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// Read the response status line. The response_ streambuf will
	// automatically grow to accommodate the entire line. The growth may be
	// limited by passing a maximum size to the streambuf constructor.
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnRequestProgress.Broadcast(bytes_sent, bytes_sent);
	});
	asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n",
	                       std::bind(&UHttpClientSsl::read_status_line, this, asio::placeholders::error, bytes_sent,
	                                 asio::placeholders::bytes_transferred)
	);
}

void UHttpClientSsl::read_status_line(const std::error_code& error, const size_t bytes_sent, const size_t bytes_recvd)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// Check that response is OK.
	OnRequestProgress.Broadcast(bytes_sent, bytes_recvd);
	std::istream response_stream(&response_buffer);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnResponseError.Broadcast(1);
		});
		return;
	}
	if (status_code != 200)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnResponseError.Broadcast(status_code);
		});
		return;
	}

	// Read the response headers, which are terminated by a blank line.
	asio::async_read_until(tcp.ssl_socket, response_buffer, "\r\n\r\n",
	                       std::bind(&UHttpClientSsl::read_headers, this, asio::placeholders::error)
	);
}

void UHttpClientSsl::read_headers(const std::error_code& error)
{
	if (error)
	{
		tcp.error_code = error;
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestError.Broadcast(error.value(), error.message().c_str());
		});
		return;
	}
	// Process the response headers.
	response.clear();
	std::istream response_stream(&response_buffer);
	std::string header;

	while (std::getline(response_stream, header) && header != "\r")
	{
		response.appendHeader(header.data());
	}

	// Write whatever content we already have to output.
	std::ostringstream content_buffer;
	if (response_buffer.size() > 0)
	{
		content_buffer << &response_buffer;
		response.setContent(content_buffer.str().data());
	}

	// Start reading remaining data until EOF.
	asio::async_read(tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
	                 std::bind(&UHttpClientSsl::read_content, this, asio::placeholders::error)
	);
}

void UHttpClientSsl::read_content(const std::error_code& error)
{
	if (error)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnRequestCompleted.Broadcast(response);
		});
		return;
	}
	// Write all of the data that has been read so far.
	std::ostringstream stream_buffer;
	stream_buffer << &response_buffer;
	if (!stream_buffer.str().empty())
	{
		response.appendContent(stream_buffer.str().data());
	}

	// Continue reading remaining data until EOF.
	asio::async_read(tcp.ssl_socket, response_buffer, asio::transfer_at_least(1),
	                 std::bind(&UHttpClientSsl::read_content, this, asio::placeholders::error)
	);
}
