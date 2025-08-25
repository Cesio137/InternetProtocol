/** 
 * Copyright © 2025 Nathan Miguel
 */

#include "websocket/wsserver.h"

#include "websocket/wsremote.h"

void UWSServer::BeginDestroy() {
	is_being_destroyed = true;
	if (net.acceptor.is_open())
		Close();
	UObject::BeginDestroy();
}

void UWSServer::AddToRoot() {
	Super::AddToRoot();
}

void UWSServer::RemoveFromRoot() {
	Super::RemoveFromRoot();
}

bool UWSServer::IsRooted() {
	return Super::IsRooted();
}

bool UWSServer::IsOpen() {
	return net.acceptor.is_open();
}

FTcpEndpoint UWSServer::LocalEndpoint() {
	tcp::endpoint ep = net.acceptor.local_endpoint();
	return FTcpEndpoint(ep);
}

const TSet<UWSRemote*>& UWSServer::Clients() {
	return net.clients;
}

FErrorCode UWSServer::GetErrorCode() {
	return FErrorCode(error_code);
}

bool UWSServer::Open(const FServerBindOptions& BindOpts) {
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

	OnListening.Broadcast();
	
	asio::post(thread_pool(), [this]{ run_context_thread(); });
	return true;
}

void UWSServer::Close() {
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
				client->Close(1000, "Shutdown server");
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

void UWSServer::run_context_thread() {
	FScopeLock lock(&mutex_io);
	error_code.clear();
	if (IsGarbageCollecting()) return;
	UWSRemote* remote_ptr = NewObject<UWSRemote>();
	remote_ptr->Construct(net.context);
	net.acceptor.async_accept(remote_ptr->get_socket(), std::bind(&UWSServer::accept, this, asio::placeholders::error, remote_ptr));
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UWSServer::accept(const asio::error_code& error, UWSRemote* remote) {
	if (error) {
		FScopeLock lock(&mutex_error);
		remote->Close(1002, "Protocol error");
		remote->Destroy();
		error_code = error;
		if (net.acceptor.is_open() && !IsGarbageCollecting()) {
			UWSRemote* remote_ptr = NewObject<UWSRemote>();
			remote_ptr->Construct(net.context);
			net.acceptor.async_accept(remote_ptr->get_socket(), std::bind(&UWSServer::accept, this, asio::placeholders::error, remote_ptr));
		}
		return;
	}
	if (!IsGarbageCollecting()) {
		net.clients.Add(remote);
		remote->on_close = [this, remote]() {
			net.clients.Remove(remote);
			remote->Destroy();
		};
		AsyncTask(ENamedThreads::GameThread, [this, remote]() {
			if (!is_being_destroyed)
				OnClientAccepted.Broadcast(remote);
		});
		remote->connect();
		if (net.acceptor.is_open()) {
			UWSRemote* remote_ptr = NewObject<UWSRemote>();
			remote_ptr->Construct(net.context);
			net.acceptor.async_accept(remote_ptr->get_socket(), std::bind(&UWSServer::accept, this, asio::placeholders::error, remote_ptr));
		}
	}
}

// SSL

void UWSServerSsl::BeginDestroy() {
	is_being_destroyed = true;
	if (net.acceptor.is_open())
		Close();
	UObject::BeginDestroy();
}

void UWSServerSsl::AddToRoot() {
	Super::AddToRoot();
}

void UWSServerSsl::RemoveFromRoot() {
	Super::RemoveFromRoot();
}

bool UWSServerSsl::IsRooted() {
	return Super::IsRooted();
}

bool UWSServerSsl::IsOpen() {
	return net.acceptor.is_open();
}

FTcpEndpoint UWSServerSsl::LocalEndpoint() {
	tcp::endpoint ep = net.acceptor.local_endpoint();
	return FTcpEndpoint(ep);
}

const TSet<UWSRemoteSsl*>& UWSServerSsl::Clients() {
	return net.ssl_clients;
}

FErrorCode UWSServerSsl::GetErrorCode() {
	return FErrorCode(error_code);
}

bool UWSServerSsl::Open(const FServerBindOptions& BindOpts) {
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

	OnListening.Broadcast();
	
	asio::post(thread_pool(), [this]{ run_context_thread(); });
	return true;
}

void UWSServerSsl::Close() {
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
				client->Close(1000, "Shutdown server");
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

void UWSServerSsl::run_context_thread() {
	FScopeLock lock(&mutex_io);
	error_code.clear();
	if (IsGarbageCollecting()) return;
	UWSRemoteSsl* remote_ptr = NewObject<UWSRemoteSsl>();
	remote_ptr->Construct(net.context, net.ssl_context);
	net.acceptor.async_accept(remote_ptr->get_socket().lowest_layer(), std::bind(&UWSServerSsl::accept, this, asio::placeholders::error, remote_ptr));
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UWSServerSsl::accept(const asio::error_code& error, UWSRemoteSsl* remote) {
	if (error) {
		FScopeLock lock(&mutex_error);
		remote->Close(1002, "Protocol error");
		remote->Destroy();
		error_code = error;
		if (net.acceptor.is_open() && !IsGarbageCollecting()) {
			UWSRemoteSsl* remote_ptr = NewObject<UWSRemoteSsl>();
			remote_ptr->Construct(net.context, net.ssl_context);
			net.acceptor.async_accept(remote_ptr->get_socket().lowest_layer(), std::bind(&UWSServerSsl::accept, this, asio::placeholders::error, remote_ptr));
		}
		return;
	}
	if (!IsGarbageCollecting()) {
		net.ssl_clients.Add(remote);
		remote->on_close = [this, remote]() {
			net.ssl_clients.Remove(remote);
			remote->Destroy();
		};
		AsyncTask(ENamedThreads::GameThread, [this, remote]() {
			if (!is_being_destroyed)
				OnClientAccepted.Broadcast(remote);
		});
		remote->connect();
		if (net.acceptor.is_open()) {
			UWSRemoteSsl* remote_ptr = NewObject<UWSRemoteSsl>();
			remote_ptr->Construct(net.context, net.ssl_context);
			net.acceptor.async_accept(remote_ptr->get_socket().lowest_layer(), std::bind(&UWSServerSsl::accept, this, asio::placeholders::error, remote_ptr));
		}
	}
}