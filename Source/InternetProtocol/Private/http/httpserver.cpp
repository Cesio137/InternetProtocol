/** 
 * Copyright © 2025 Nathan Miguel
 */

#include "http/httpserver.h"

void UHttpServer::BeginDestroy() {
	is_being_destroyed = true;
	if (net.acceptor.is_open())
		Close();
	UObject::BeginDestroy();
}

void UHttpServer::AddToRoot() {
	Super::AddToRoot();
}

void UHttpServer::RemoveFromRoot() {
	Super::RemoveFromRoot();
}

bool UHttpServer::IsRooted() {
	return Super::IsRooted();
}

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
		if (!is_being_destroyed)
			OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	net.acceptor.set_option(asio::socket_base::reuse_address(BindOpts.bReuse_Address), error_code);
	if (error_code) {
		FScopeLock lock(&mutex_error);
		if (!is_being_destroyed)
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
		if (!is_being_destroyed)
			OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	net.acceptor.listen(Backlog, error_code);
	if (error_code) {
		FScopeLock lock(&mutex_error);
		if (!is_being_destroyed)
			OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	asio::post(thread_pool(), [this]{ run_context_thread(); });
	return true;
}

void UHttpServer::Close() {
	is_closing.Store(true);
	if (net.acceptor.is_open()) {
		FScopeLock lock(&mutex_error);
		net.acceptor.close(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				if (!is_being_destroyed)
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
	AsyncTask(ENamedThreads::GameThread, [this]() {
		if (!is_being_destroyed)
			OnClose.Broadcast();
	});
	is_closing.Store(false);
}

void UHttpServer::run_context_thread() {
	FScopeLock lock(&mutex_io);
	error_code.clear();
	if (IsGarbageCollecting()) return;
	UHttpRemote* remote_ptr = NewObject<UHttpRemote>();
	remote_ptr->Construct(net.context, IdleTimeoutSeconds);
	net.acceptor.async_accept(remote_ptr->get_socket(), std::bind(&UHttpServer::accept, this, asio::placeholders::error, remote_ptr));
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UHttpServer::accept(const asio::error_code& error, UHttpRemote* remote) {
	if (error) {
		FScopeLock lock(&mutex_error);
		remote->Close();
		remote->Destroy();
		error_code = error;
		if (net.acceptor.is_open() && !IsGarbageCollecting()) {
			UHttpRemote* remote_ptr = NewObject<UHttpRemote>();
			remote_ptr->Construct(net.context, IdleTimeoutSeconds);
			net.acceptor.async_accept(remote_ptr->get_socket(), std::bind(&UHttpServer::accept, this, asio::placeholders::error, remote_ptr));
		}
		return;
	}
	if (!IsGarbageCollecting()) {
		remote->on_request = [this, remote](const FHttpRequest& request) {
			read_cb(request, remote);
		};
		remote->on_close = [this, remote]() {
			net.clients.Remove(remote);
			remote->Destroy();
		};
		remote->connect();
		net.clients.Add(remote);
		if (net.acceptor.is_open()) {
			UHttpRemote* remote_ptr = NewObject<UHttpRemote>();
			remote_ptr->Construct(net.context, IdleTimeoutSeconds);
			net.acceptor.async_accept(remote_ptr->get_socket(), std::bind(&UHttpServer::accept, this, asio::placeholders::error, remote_ptr));
		}
	}
}

void UHttpServer::read_cb(const FHttpRequest& request, UHttpRemote* remote) {
	if (is_being_destroyed) return;
	if (all_cb.Contains(request.Path)) {
		AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
			all_cb[request.Path].ExecuteIfBound(request, remote);
		});
	}
	switch (request.Method) {
	case ERequestMethod::DEL:
    	if (del_cb.Contains(request.Path) && !is_being_destroyed) {
    		AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
    			del_cb[request.Path].ExecuteIfBound(request, remote);
			});    		
    	}
    	break;
    case ERequestMethod::GET:
        if (get_cb.Contains(request.Path)) {
        	AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
        		get_cb[request.Path].ExecuteIfBound(request, remote);
			});
        }
        break;
    case ERequestMethod::HEAD:
        if (head_cb.Contains(request.Path)) {
        	AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
				head_cb[request.Path].ExecuteIfBound(request, remote);
			});
        }
        break;
    case ERequestMethod::OPTIONS:
        if (options_cb.Contains(request.Path)) {
        	AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
				options_cb[request.Path].ExecuteIfBound(request, remote);
			});
        }
        break;
    case ERequestMethod::POST:
        if (post_cb.Contains(request.Path)) {
        	AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
        		post_cb[request.Path].ExecuteIfBound(request, remote);
			});
        }
        break;
    case ERequestMethod::PUT:
        if (put_cb.Contains(request.Path)) {
        	AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
        		put_cb[request.Path].ExecuteIfBound(request, remote);
			});
        	
        }
        break;
    case ERequestMethod::PATCH:
        if (patch_cb.Contains(request.Path)) {
        	AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
        		patch_cb[request.Path].ExecuteIfBound(request, remote);
			});
        }
        break;
    default: break;
	}
}

void UHttpServerSsl::BeginDestroy() {
	is_being_destroyed = true;
	if (net.acceptor.is_open())
		Close();
	UObject::BeginDestroy();
}

void UHttpServerSsl::AddToRoot() {
	Super::AddToRoot();
}

void UHttpServerSsl::RemoveFromRoot() {
	Super::RemoveFromRoot();
}

bool UHttpServerSsl::IsRooted() {
	return Super::IsRooted();
}

bool UHttpServerSsl::IsOpen() {
	return net.acceptor.is_open();
}

FTcpEndpoint UHttpServerSsl::LocalEndpoint() {
	tcp::endpoint ep = net.acceptor.local_endpoint();
	return FTcpEndpoint(ep);
}

TSet<UHttpRemoteSsl*>& UHttpServerSsl::Clients() {
	return net.ssl_clients;
}

FErrorCode UHttpServerSsl::GetErrorCode() {
	return FErrorCode(error_code);
}

void UHttpServerSsl::All(const FString& Path, const FDelegateHttpServerRequestSsl& Callback) {
	all_cb.Add(Path, Callback);
}

void UHttpServerSsl::Get(const FString& Path, const FDelegateHttpServerRequestSsl& Callback) {
	get_cb.Add(Path, Callback);
}

void UHttpServerSsl::Post(const FString& Path, const FDelegateHttpServerRequestSsl& Callback) {
	post_cb.Add(Path, Callback);
}

void UHttpServerSsl::Put(const FString& Path, const FDelegateHttpServerRequestSsl& Callback) {
	put_cb.Add(Path, Callback);
}

void UHttpServerSsl::Del(const FString& Path, const FDelegateHttpServerRequestSsl& Callback) {
	del_cb.Add(Path, Callback);
}

void UHttpServerSsl::Head(const FString& Path, const FDelegateHttpServerRequestSsl& Callback) {
	head_cb.Add(Path, Callback);
}

void UHttpServerSsl::Options(const FString& Path, const FDelegateHttpServerRequestSsl& Callback) {
	options_cb.Add(Path, Callback);
}

void UHttpServerSsl::Patch(const FString& Path, const FDelegateHttpServerRequestSsl& Callback) {
	patch_cb.Add(Path, Callback);
}

bool UHttpServerSsl::Open(const FServerBindOptions& BindOpts) {
	if (net.acceptor.is_open())
		return false;

	net.acceptor.open(BindOpts.Protocol == EProtocolType::V4 
						  ? tcp::v4()
						  : tcp::v6(),
						  error_code);

	if (error_code) {
		FScopeLock lock(&mutex_error);
		if (!is_being_destroyed)
			OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	net.acceptor.set_option(asio::socket_base::reuse_address(BindOpts.bReuse_Address), error_code);
	if (error_code) {
		FScopeLock lock(&mutex_error);
		if (!is_being_destroyed)
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
		if (!is_being_destroyed)
			OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	net.acceptor.listen(Backlog, error_code);
	if (error_code) {
		FScopeLock lock(&mutex_error);
		if (!is_being_destroyed)
			OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	asio::post(thread_pool(), [this]{ run_context_thread(); });
	return true;
}

void UHttpServerSsl::Close() {
	is_closing.Store(true);
	if (net.acceptor.is_open()) {
		FScopeLock lock(&mutex_error);
		net.acceptor.close(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [this]() {
				if (!is_being_destroyed)
					OnError.Broadcast(FErrorCode(error_code));
			});
	}
	if (net.ssl_clients.Num()) {
		FScopeLock lock(&mutex_error);
		for (const auto &client : net.ssl_clients) {
			if (client)
				client->Close();
		}
		net.ssl_clients.Empty();
		net.ssl_clients.Shrink();
	}
	net.context.stop();
	net.context.restart();
	net.acceptor = tcp::acceptor(net.context);
	AsyncTask(ENamedThreads::GameThread, [this]() {
		if (!is_being_destroyed)
			OnClose.Broadcast();
	});
	is_closing.Store(false);
}

void UHttpServerSsl::run_context_thread() {
	FScopeLock lock(&mutex_io);
	error_code.clear();
	if (IsGarbageCollecting()) return;
	UHttpRemoteSsl* remote_ptr = NewObject<UHttpRemoteSsl>();
	remote_ptr->Construct(net.context, net.ssl_context, IdleTimeoutSeconds);
	net.acceptor.async_accept(remote_ptr->get_socket().lowest_layer(), std::bind(&UHttpServerSsl::accept, this, asio::placeholders::error, remote_ptr));
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UHttpServerSsl::accept(const asio::error_code& error, UHttpRemoteSsl* remote) {
	if (error) {
		FScopeLock lock(&mutex_error);
		remote->Close();
		remote->Destroy();
		error_code = error;
		if (net.acceptor.is_open() && !IsGarbageCollecting()) {
			UHttpRemoteSsl* remote_ptr = NewObject<UHttpRemoteSsl>();
			remote_ptr->Construct(net.context, net.ssl_context, IdleTimeoutSeconds);
			net.acceptor.async_accept(remote_ptr->get_socket().lowest_layer(), std::bind(&UHttpServerSsl::accept, this, asio::placeholders::error, remote_ptr));
		}
		return;
	}
	if (!IsGarbageCollecting()) {
		remote->on_request = [this, remote](const FHttpRequest& request) {
			read_cb(request, remote);
		};
		remote->on_close = [this, remote]() {
			net.ssl_clients.Remove(remote);
			remote->Destroy();
		};
		remote->connect();
		net.ssl_clients.Add(remote);
		if (net.acceptor.is_open()) {
			UHttpRemoteSsl* remote_ptr = NewObject<UHttpRemoteSsl>();
			remote_ptr->Construct(net.context, net.ssl_context, IdleTimeoutSeconds);
			net.acceptor.async_accept(remote_ptr->get_socket().lowest_layer(), std::bind(&UHttpServerSsl::accept, this, asio::placeholders::error, remote_ptr));
		}
	}
}

void UHttpServerSsl::read_cb(const FHttpRequest& request, UHttpRemoteSsl* remote) {
	if (is_being_destroyed) return;
	if (all_cb.Contains(request.Path)) {
		all_cb[request.Path].ExecuteIfBound(request, remote);
	}
	switch (request.Method) {
	case ERequestMethod::DEL:
		if (del_cb.Contains(request.Path)) {
			AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
				del_cb[request.Path].ExecuteIfBound(request, remote);
			});
		}
		break;
	case ERequestMethod::GET:
		if (get_cb.Contains(request.Path)) {
			AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
				get_cb[request.Path].ExecuteIfBound(request, remote);
			});
		}
		break;
	case ERequestMethod::HEAD:
		if (head_cb.Contains(request.Path)) {
			AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
				head_cb[request.Path].ExecuteIfBound(request, remote);
			});
		}
		break;
	case ERequestMethod::OPTIONS:
		if (options_cb.Contains(request.Path)) {
			AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
				options_cb[request.Path].ExecuteIfBound(request, remote);
			});
		}
		break;
	case ERequestMethod::POST:
		if (post_cb.Contains(request.Path)) {
			AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
				post_cb[request.Path].ExecuteIfBound(request, remote);
			});
		}
		break;
	case ERequestMethod::PUT:
		if (put_cb.Contains(request.Path)) {
			AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
				put_cb[request.Path].ExecuteIfBound(request, remote);
			});
		}
		break;
	case ERequestMethod::PATCH:
		if (patch_cb.Contains(request.Path)) {
			AsyncTask(ENamedThreads::GameThread, [this, request, remote]() {
				patch_cb[request.Path].ExecuteIfBound(request, remote);
			});
		}
		break;
	default: break;
	}
}
