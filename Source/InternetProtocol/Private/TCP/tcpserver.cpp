/** 
 * Copyright © 2025 Nathan Miguel
 */


#include "tcp/tcpserver.h"

void UTCPServer::BeginDestroy() {
	is_being_destroyed = true;
	if (net.acceptor.is_open())
		Close();
	UObject::BeginDestroy();
}

void UTCPServer::AddToRoot() {
	Super::AddToRoot();
}

void UTCPServer::RemoveFromRoot() {
	Super::RemoveFromRoot();
}

bool UTCPServer::IsRooted() {
	return Super::IsRooted();
}

void UTCPServer::MarkPendingKill() {
	Super::MarkPendingKill();
}

bool UTCPServer::IsOpen() {
	return net.acceptor.is_open();
}

FTcpEndpoint UTCPServer::LocalEndpoint() {
	tcp::endpoint ep = net.acceptor.local_endpoint();
	return FTcpEndpoint(ep);
}

const TSet<UTCPRemote*>& UTCPServer::Clients() {
	return net.clients;
}

FErrorCode UTCPServer::GetErrorCode() {
	return FErrorCode(error_code);
}

bool UTCPServer::Open(const FServerBindOptions& BindOpts) {
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

void UTCPServer::Close() {
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

void UTCPServer::run_context_thread() {
	FScopeLock lock(&mutex_io);
	error_code.clear();
	TSharedPtr<tcp::socket> socket_ptr = MakeShared<tcp::socket>(net.context);
	net.acceptor.async_accept(*socket_ptr, std::bind(&UTCPServer::accept, this, asio::placeholders::error, socket_ptr));
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UTCPServer::accept(const asio::error_code& error, TSharedPtr<tcp::socket>& socket) {
	if (error) {
		FScopeLock lock(&mutex_error);
		if (!is_closing.Load())
			socket->close(error_code);
		error_code = error;
		if (net.acceptor.is_open()) {
			TSharedPtr<tcp::socket> socket_ptr = MakeShared<tcp::socket>(net.context);
			net.acceptor.async_accept(*socket_ptr, std::bind(&UTCPServer::accept, this, asio::placeholders::error, socket_ptr));
		}
		return;
	}
	UTCPRemote *client = NewObject<UTCPRemote>();
	client->Construct(socket);
	client->connect();
	net.clients.Add(client);
	client->on_close = [this, client]() {
		net.clients.Remove(client);
		client->Destroy();
	};
	AsyncTask(ENamedThreads::GameThread, [this, client]() {
		if (!is_being_destroyed)
			OnClientAccepted.Broadcast(client);
	});	
	if (net.acceptor.is_open()) {
		TSharedPtr<tcp::socket> socket_ptr = MakeShared<tcp::socket>(net.context);
		net.acceptor.async_accept(*socket_ptr, std::bind(&UTCPServer::accept, this, asio::placeholders::error, socket_ptr));
	}
}

void UTCPServerSsl::BeginDestroy() {
	is_being_destroyed = true;
	if (net.acceptor.is_open())
		Close();
	UObject::BeginDestroy();
}

void UTCPServerSsl::AddToRoot() {
	Super::AddToRoot();
}

void UTCPServerSsl::RemoveFromRoot() {
	Super::RemoveFromRoot();
}

bool UTCPServerSsl::IsRooted() {
	return Super::IsRooted();
}

void UTCPServerSsl::MarkPendingKill() {
	Super::MarkPendingKill();
}

bool UTCPServerSsl::IsOpen() {
	return net.acceptor.is_open();
}

FTcpEndpoint UTCPServerSsl::LocalEndpoint() {
	tcp::endpoint ep = net.acceptor.local_endpoint();
	return FTcpEndpoint(ep);
}

const TSet<UTCPRemoteSsl*>& UTCPServerSsl::Clients() {
	return net.ssl_clients;
}

FErrorCode UTCPServerSsl::GetErrorCode() {
	return FErrorCode(error_code);
}

bool UTCPServerSsl::Open(const FServerBindOptions& BindOpts) {
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

void UTCPServerSsl::Close() {
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

void UTCPServerSsl::run_context_thread() {
	FScopeLock lock(&mutex_error);
	error_code.clear();
	TSharedPtr<asio::ssl::stream<tcp::socket>> socket_ptr = MakeShared<asio::ssl::stream<tcp::socket>>(net.context, net.ssl_context);
	net.acceptor.async_accept(socket_ptr->lowest_layer(), std::bind(&UTCPServerSsl::accept, this, asio::placeholders::error, socket_ptr));
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UTCPServerSsl::accept(const asio::error_code& error, TSharedPtr<asio::ssl::stream<tcp::socket>>& socket) {
	if (error) {
		FScopeLock lock(&mutex_error);
		if (!is_closing.Load())
			socket->next_layer().close(error_code);
		error_code = error;
		if (net.acceptor.is_open()) {
			TSharedPtr<asio::ssl::stream<tcp::socket>> socket_ptr = MakeShared<asio::ssl::stream<tcp::socket>>(net.context, net.ssl_context);
			net.acceptor.async_accept(socket_ptr->lowest_layer(), std::bind(&UTCPServerSsl::accept, this, asio::placeholders::error, socket_ptr));
		}
		return;
	}
	UTCPRemoteSsl* client = NewObject<UTCPRemoteSsl>();
	client->Construct(socket);
	client->connect();
	net.ssl_clients.Add(client);
	client->on_close = [this, client]() {
		net.ssl_clients.Remove(client);
		client->Destroy();
	};
	AsyncTask(ENamedThreads::GameThread, [this, client]() {
		if (!is_being_destroyed)
			OnClientAccepted.Broadcast(client);
	});
	if (net.acceptor.is_open()) {
		TSharedPtr<asio::ssl::stream<tcp::socket>> socket_ptr = MakeShared<asio::ssl::stream<tcp::socket>>(net.context, net.ssl_context);
		net.acceptor.async_accept(socket_ptr->lowest_layer(), std::bind(&UTCPServerSsl::accept, this, asio::placeholders::error, socket_ptr));
	}
}
