// Fill out your copyright notice in the Description page of Project Settings.


#include "http/httpserver.h"

bool UHttpServer::IsOpen() {
	return net.acceptor.is_open();
}

FTcpEndpoint UHttpServer::LocalEndpoint() {
	tcp::endpoint ep = net.acceptor.local_endpoint();
	return FTcpEndpoint(ep);
}

TSet<UHttpRemote*>& UHttpServer::Clients() {
	return net.clients;
}

FErrorCode UHttpServer::GetErrorCode() {
	return FErrorCode(error_code);
}

void UHttpServer::SetMaxConnections(int Val) {
	max_connections = Val < 0 ? 0 : Val;
}

int UHttpServer::GetMaxConnections() {
	return max_connections;
}

void UHttpServer::SetIdleTimeout(uint8 Val) {
	iddle_timeout = Val;
}

uint8 UHttpServer::GetIdleTimeout() {
	return iddle_timeout;
}

void UHttpServer::All(const FString& Path, const FDelegateHttpServerRequest& Callback) {
	all_cb.Add(Path, Callback);
}

void UHttpServer::Get(const FString& Path, const FDelegateHttpServerRequest& Callback) {
	get_cb.Add(Path, Callback);
}

void UHttpServer::Post(const FString& Path, const FDelegateHttpServerRequest& Callback) {
	post_cb.Add(Path, Callback);
}

void UHttpServer::Put(const FString& Path, const FDelegateHttpServerRequest& Callback) {
	put_cb.Add(Path, Callback);
}

void UHttpServer::Del(const FString& Path, const FDelegateHttpServerRequest& Callback) {
	del_cb.Add(Path, Callback);
}

void UHttpServer::Head(const FString& Path, const FDelegateHttpServerRequest& Callback) {
	head_cb.Add(Path, Callback);
}

void UHttpServer::Options(const FString& Path, const FDelegateHttpServerRequest& Callback) {
	options_cb.Add(Path, Callback);
}

void UHttpServer::Patch(const FString& Path, const FDelegateHttpServerRequest& Callback) {
	patch_cb.Add(Path, Callback);
}

bool UHttpServer::Open(const FServerBindOptions& BindOpts) {
	if (net.acceptor.is_open())
		return false;

	net.acceptor.open(BindOpts.Protocol == EProtocolType::V4 
						  ? tcp::v4()
						  : tcp::v6(),
						  error_code);

	if (error_code) {
		FScopeLock lock(&mutex_error);
		OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	net.acceptor.set_option(asio::socket_base::reuse_address(BindOpts.bReuse_Address), error_code);
	if (error_code) {
		FScopeLock lock(&mutex_error);
		if (error_code)
			OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	const tcp::endpoint endpoint = BindOpts.Address.IsEmpty() ?
								tcp::endpoint(BindOpts.Protocol == EProtocolType::V4
												? tcp::v4()
												: tcp::v6(),
												BindOpts.Port) :
								tcp::endpoint(make_address(TCHAR_TO_UTF8(*BindOpts.Address)), BindOpts.Port);

	net.acceptor.bind(endpoint, error_code);
	if (error_code) {
		FScopeLock lock(&mutex_error);
		if (error_code)
			OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	net.acceptor.listen(max_connections, error_code);
	if (error_code) {
		FScopeLock lock(&mutex_error);
		if (error_code)
			OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	asio::post(thread_pool(), [&]{ run_context_thread(); });
	return true;
}

void UHttpServer::Close() {
	is_closing.Store(true);
	if (net.acceptor.is_open()) {
		FScopeLock lock(&mutex_error);
		net.acceptor.close(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [&]() {
				OnError.Broadcast(FErrorCode(error_code));
			});
	}
	if (net.clients.Num()) {
		FScopeLock lock(&mutex_error);
		for (const auto &client : net.clients) {
			if (client)
				client->Close();
		}
		net.clients.Empty();
		net.clients.Shrink();
	}
	net.context.stop();
	net.context.restart();
	net.acceptor = tcp::acceptor(net.context);
	AsyncTask(ENamedThreads::GameThread, [&]() {
		OnClose.Broadcast();
	});
	is_closing.Store(false);
}

void UHttpServer::run_context_thread() {
	FScopeLock lock(&mutex_io);
	error_code.clear();
	UHttpRemote *client_socket = NewObject<UHttpRemote>();
	client_socket->Construct(net.context);
	net.acceptor.async_accept(client_socket->get_socket(),
								[&, client_socket](const asio::error_code &ec) {
									accept(ec, client_socket);
								});
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UHttpServer::accept(const asio::error_code& error, UHttpRemote* client) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		if (!is_closing.Load())
			client->Close();
		if (net.acceptor.is_open()) {
			UHttpRemote *client_socket = NewObject<UHttpRemote>();
			client_socket->Construct(net.context);
			net.acceptor.async_accept(client_socket->get_socket(),
										[&, client_socket](const asio::error_code &ec) {
											accept(ec, client_socket);
										});
		}
		return;
	}
	if (net.clients.Num() < max_connections) {
		client->on_request = [&, client](const FHttpRequest &request) {
			read_cb(request, client);
		};
		client->on_close = [&, client]() {
			net.clients.Remove(client);
		};
		client->connect();
		net.clients.Add(client);
	} else {
		FScopeLock lock(&mutex_error);
		if (!is_closing.Load())
			client->Close();
	}
	if (net.acceptor.is_open()) {
		UHttpRemote *client_socket = NewObject<UHttpRemote>();
		client_socket->Construct(net.context);
		net.acceptor.async_accept(client_socket->get_socket(),
								[&, client_socket](const asio::error_code &ec) {
									accept(ec, client_socket);
								});
	}
}

void UHttpServer::read_cb(const FHttpRequest& request, UHttpRemote* client) {
	if (all_cb.Contains(request.Path)) {
		all_cb[request.Path].ExecuteIfBound(request, client);
	}
	switch (request.Method) {
	case ERequestMethod::DEL:
    	if (del_cb.Contains(request.Path)) {
    		del_cb[request.Path].ExecuteIfBound(request, client);
    	}
    	break;
    case ERequestMethod::GET:
        if (get_cb.Contains(request.Path)) {
        	get_cb[request.Path].ExecuteIfBound(request, client);
        }
        break;
    case ERequestMethod::HEAD:
        if (head_cb.Contains(request.Path)) {
        	head_cb[request.Path].ExecuteIfBound(request, client);
        }
        break;
    case ERequestMethod::OPTIONS:
        if (options_cb.Contains(request.Path)) {
        	options_cb[request.Path].ExecuteIfBound(request, client);
        }
        break;
    case ERequestMethod::POST:
        if (post_cb.Contains(request.Path)) {
        	post_cb[request.Path].ExecuteIfBound(request, client);
        }
        break;
    case ERequestMethod::PUT:
        if (put_cb.Contains(request.Path)) {
        	put_cb[request.Path].ExecuteIfBound(request, client);
        }
        break;
    case ERequestMethod::PATCH:
        if (patch_cb.Contains(request.Path)) {
        	patch_cb[request.Path].ExecuteIfBound(request, client);
        }
        break;
    default: break;
	}
}
