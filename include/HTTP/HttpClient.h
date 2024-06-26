#pragma once

#include <Net/Common.h>
#include <iostream>

namespace InternetProtocol {
	class HttpClient
	{
	public:
		HttpClient() {
			request.headers.insert_or_assign("Accept", "*/*");
			request.headers.insert_or_assign("User-Agent", "ASIO 2.30.2");
			request.headers.insert_or_assign("Connection", "close");
		}
		~HttpClient() {
			clearRequest();
			clearPayload();
			clearResponse();
		}

		/*HTTP SETTINGS*/

		void setHost(const std::string& url = "localhost", const std::string& port = "")
		{
			host = url;
			service = port;
		}
		std::string getHost() const { return host; }
		std::string getPort() const { return service; }

		void setRetryTime(int value = 3.) { retrytime = value; }
		void resetRetryTime() { retrytime = 5; }
		int getRetryTime() const { return retrytime; }

		void setMaxRetry(int value = 1) { maxretry = value; }
		void resetMaxRetry() { maxretry = 1; }
		int getMaxRetry() const { return  maxretry; }
		
		/*REQUEST DATA*/
		void setRequest(const FRequest& value) { request = value; }
		FRequest getRequest() const { return request; }
		
		void setRequestMethod(EVerb requestMethod = EVerb::GET) { request.verb = requestMethod; }
		EVerb getRequestMethod() const { return request.verb; }

		void setVersion(const std::string& version = "2.0")	{ request.version = version; }
		std::string getVersion() const { return request.version; }

		void setPath(const std::string& path = "/") { request.path = path.empty() ? "/" : path; }
		std::string getPath() const { return request.path; }

		void appendParams(const std::string& key, const std::string& value) { request.params[key] = value; }
		void clearParams() { request.params.clear(); }
		void removeParam(const std::string& key) { request.params.erase(key); }
		bool hasParam(const std::string& key) const { return request.params.contains(key); }
		std::map<std::string, std::string> getParams() const { return request.params; }

		void AppendHeaders(const std::string& key, const std::string& value) { request.headers[key] = value; }
		void ClearHeaders() { request.headers.clear(); }
		void RemoveHeader(const std::string& key) { request.headers.erase(key); }
		bool hasHeader(const std::string& key) const { return request.headers.contains(key); }
		std::map<std::string, std::string> GetHeaders() const { return request.headers; }

		void SetBody(const std::string& value) { request.body = value; }
		void ClearBody() { request.body.clear(); }
		std::string GetBody() const { return request.body; }

		FRequest getRequestData() const { return request; }

		/*PAYLOAD*/
		void preparePayload()
		{
			if (!payload.empty())
				payload.clear();

			payload = verb.at(request.verb) + " " + request.path;

			if (!request.params.empty())
			{
				payload += "?";
				bool first = true;
				for (const std::pair<std::string, std::string> param : request.params) {
					if (!first) payload += "&";
					payload += param.first + "=" + param.second;
					first = false;
				}
			}
			payload += " HTTP/" + request.version + "\r\n";

			payload += "Host: " + host;
			if (!service.empty()) payload += ":" + service;
			payload += "\r\n";

			if (!request.headers.empty())
			{
				for (const std::pair<std::string, std::string> header : request.headers) {
					payload += header.first + ": " + header.second + "\r\n";
				}
				payload += "Content-Length: " + std::to_string(request.body.length()) + "\r\n";
			}
			payload += "\r\n";

			if (!request.body.empty())
				payload += "\r\n" + request.body;
		}

		void async_preparePayload()
		{
			asio::post(*pool, [this](){
				mutexPayload.lock();
				preparePayload();
				if (onAsyncPayloadFinished)
					onAsyncPayloadFinished();
				mutexPayload.unlock();
			});
		}

		std::string getPayloadData() const { return payload; }

		/*RESPONSE DATA*/
		FResponse getResponseData() const { return response; }

		/*THREADS*/
		void setThreadNumber(int value = 2) { pool = std::make_unique<asio::thread_pool>(value); }

		/*CONNECTION*/
		int processRequest()
		{
			if (!pool)
				return -1;

			asio::post(*pool, [this](){
				runContextThread();
			});
			
			return 0;
		}

		void cancelRequest() { tcp.context.stop(); }

		/*MEMORY MANAGER*/
		void clearRequest() { request.clear(); }
		void clearPayload() { payload.clear(); }
		void clearResponse() { response.clear(); }

		/*ERRORS*/
		int getErrorCode() const { return tcp.error_code.value(); }
		std::string getErrorMessage() const { return tcp.error_code.message(); }

		/*EVENTS*/
		std::function<void()> onConnected;
		std::function<void()> onAsyncPayloadFinished;
		std::function<void(const FResponse)> onRequestComplete;
		std::function<void(int, int)> onRequestProgress;
		std::function<void(int, int)> onRequestWillRetry;
		std::function<void(int, const std::string&)> onRequestFail;
		std::function<void(EStatusCode)> onResponseFail;
		
	private:
		std::unique_ptr<asio::thread_pool> pool = std::make_unique<asio::thread_pool>(2);
		std::mutex mutexPayload;
		std::mutex mutexIO;
		std::string host = "localhost";
		std::string service;
		int maxretry = 1;
		int retrytime = 3;
		FRequest request;
		FAsioTcp tcp;
		std::string payload;
		asio::streambuf request_buffer;
		asio::streambuf response_buffer;
		FResponse response;
		int bytes_sent = 0;
		int bytes_received = 0;
		const std::map<EVerb, std::string> verb = {
			{EVerb::GET     , "GET"},
			{EVerb::POST    , "POST"},
			{EVerb::PUT     , "PUT"},
			{EVerb::PATCH   , "PATCH"},
			{EVerb::DEL     , "DELETE"},
			{EVerb::COPY    , "COPY"},
			{EVerb::HEAD    , "HEAD"},
			{EVerb::OPTIONS , "OPTIONS"},
			{EVerb::LOCK    , "LOCK"},
			{EVerb::UNLOCK  , "UNLOCK"},
			{EVerb::PROPFIND, "PROPFIND"},
		};

		void runContextThread() {
			mutexIO.lock();
			tcp.context.restart();
			tcp.resolver.async_resolve(getHost(),
				getPort(),
				std::bind(&HttpClient::resolve, this, asio::placeholders::error, asio::placeholders::results)
			);				
			tcp.context.run();
			clearStreamBuffers();
			if (getErrorCode() != 0 && maxretry > 0 && retrytime > 0) {
				int i = 0;
				while (getErrorCode() != 0 && i < maxretry) {
					i++;
					if (onRequestWillRetry)
						onRequestWillRetry(i, retrytime);
					std::this_thread::sleep_for(std::chrono::seconds(retrytime));
					tcp.context.restart();
					tcp.resolver.async_resolve(getHost(), getPort(),
						std::bind(&HttpClient::resolve, this, asio::placeholders::error, asio::placeholders::results)
					);
					tcp.context.run();
				}
				clearStreamBuffers();
			}
			tcp.socket.close();
			mutexIO.unlock();
		}

		void clearStreamBuffers()
		{
			request_buffer.consume(request_buffer.size());
			response_buffer.consume(request_buffer.size());
			bytes_sent = 0;
			bytes_received = 0;
		}
		
		void resolve(const std::error_code& err, const asio::ip::tcp::resolver::results_type& endpoints)
		{
			if (err)
			{
				tcp.error_code = err;
				if (onRequestFail)
					onRequestFail(tcp.error_code.value(), tcp.error_code.message());
				return;
			}
			// Attempt a connection to each endpoint in the list until we
			// successfully establish a connection.
			tcp.endpoints = endpoints;

			asio::async_connect(tcp.socket,
				tcp.endpoints,
				std::bind(&HttpClient::connect, this, asio::placeholders::error)
			);
		}
		void connect(const std::error_code& err)
		{
			if (err)
			{
				tcp.error_code = err;
				if (onRequestFail)
					onRequestFail(tcp.error_code.value(), tcp.error_code.message());
				return;
			}
			if (onConnected)
				onConnected();
			// The connection was successful. Send the request.;
			std::ostream request_stream(&request_buffer);
			request_stream << payload;
			asio::async_write(tcp.socket,
				request_buffer,
				std::bind(&HttpClient::write_request, this, asio::placeholders::error)
			);
		}
		void write_request(const std::error_code& err)
		{
			if (err)
			{
				tcp.error_code = err;
				if (onRequestFail)
					onRequestFail(tcp.error_code.value(), tcp.error_code.message());
				return;
			}
			// Read the response status line. The response_ streambuf will
			// automatically grow to accommodate the entire line. The growth may be
			// limited by passing a maximum size to the streambuf constructor.
			bytes_sent = request_buffer.size();
			if (onRequestProgress)
				onRequestProgress(bytes_sent, bytes_sent);
			asio::async_read_until(tcp.socket,
				response_buffer, "\r\n",
				std::bind(&HttpClient::read_status_line, this, asio::placeholders::error)
			);
		}
		void read_status_line(const std::error_code& err)
		{
			if (err)
			{
				tcp.error_code = err;
				if (onRequestFail)
					onRequestFail(tcp.error_code.value(), tcp.error_code.message());
				return;
			}
			// Check that response is OK.
			bytes_received = response_buffer.size();
			if (onRequestProgress)
				onRequestProgress(bytes_sent, bytes_received);
			std::istream response_stream(&response_buffer);
			std::string http_version;
			response_stream >> http_version;
			unsigned int status_code;
			response_stream >> status_code;
			std::string status_message;
			std::getline(response_stream, status_message);
			if (!response_stream || http_version.substr(0, 5) != "HTTP/")
			{
				if (onResponseFail)
					onResponseFail(EStatusCode::Invalid_0);
				return;
			}
			if (status_code != 200)
			{
				if (onResponseFail)
					onResponseFail(static_cast<EStatusCode>(status_code));
				return;
			}

			// Read the response headers, which are terminated by a blank line.
			asio::async_read_until(tcp.socket,
				response_buffer, "\r\n\r\n",
				std::bind(&HttpClient::read_headers, this, asio::placeholders::error)
			);
		}
		void read_headers(const std::error_code& err)
		{
			if (err)
			{
				tcp.error_code = err;
				if (onRequestFail)
					onRequestFail(tcp.error_code.value(), tcp.error_code.message());
				return;
			}
			// Process the response headers.
			response.clear();
			std::istream response_stream(&response_buffer);
			std::string header;

			while (std::getline(response_stream, header) && header != "\r")
				response.appendHeader(header);

			// Write whatever content we already have to output.
			std::ostringstream content_buffer;
			if (response_buffer.size() > 0) {
				content_buffer << &response_buffer;
				response.setContent(content_buffer.str());
			}

			// Start reading remaining data until EOF.
			asio::async_read(tcp.socket,
				response_buffer,
				asio::transfer_at_least(1),
				std::bind(&HttpClient::read_content, this, asio::placeholders::error)
			);
		}
		void read_content(const std::error_code& err)
		{
			if (err)
			{
				if (onRequestComplete)
					onRequestComplete(response);
				return;
			}
			// Write all of the data that has been read so far.
			std::ostringstream stream_buffer;
			stream_buffer << &response_buffer;
			std::cout << "stream_Buffer: " << stream_buffer.str() << std::endl;
			if (!stream_buffer.str().empty())
				response.setContent(stream_buffer.str());


			// Continue reading remaining data until EOF.
			asio::async_read(tcp.socket,
				response_buffer,
				asio::transfer_at_least(1),
				std::bind(&HttpClient::read_content, this, asio::placeholders::error)
			);
		}
	};
}