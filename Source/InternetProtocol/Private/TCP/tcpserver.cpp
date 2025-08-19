/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
 */


#include "tcp/tcpserver.h"

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

void UTCPServer::SetMaxConnections(int Val) {
	max_connections = Val < 0 ? 0 : Val;
}

int UTCPServer::GetMaxConnections() {
	return max_connections;
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

void UTCPServer::Close() {
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

void UTCPServer::run_context_thread() {
	FScopeLock lock(&mutex_io);
	error_code.clear();
	UTCPRemote *client_socket = NewObject<UTCPRemote>();
	client_socket->Construct(net.context);
	net.acceptor.async_accept(client_socket->get_socket(),
								[&, client_socket](const asio::error_code &ec) {
									accept(ec, client_socket);
								});
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UTCPServer::accept(const asio::error_code& error, UTCPRemote* client) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		if (!is_closing.Load())
			client->Close();
		if (net.acceptor.is_open()) {
			UTCPRemote *client_socket = NewObject<UTCPRemote>();
			client_socket->Construct(net.context);
			net.acceptor.async_accept(client_socket->get_socket(),
										[&, client_socket](const asio::error_code &ec) {
											accept(ec, client_socket);
										});
		}
		return;
	}
	if (net.clients.Num() < max_connections) {
		client->connect();
		net.clients.Add(client);
		client->on_close = [&, client]() {
			net.clients.Remove(client);
		};
		OnClientAccepted.Broadcast(client);
	} else {
		FScopeLock lock(&mutex_error);
		if (!is_closing.Load())
			client->Close();
	}
	if (net.acceptor.is_open()) {
		UTCPRemote *client_socket = NewObject<UTCPRemote>();
		client_socket->Construct(net.context);
		net.acceptor.async_accept(client_socket->get_socket(),
								[&, client_socket](const asio::error_code &ec) {
									accept(ec, client_socket);
								});
	}
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

void UTCPServerSsl::SetMaxConnections(int Val) {
	max_connections = Val < 0 ? 0 : Val;
}

int UTCPServerSsl::GetMaxConnections() {
	return max_connections;
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
		OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	net.acceptor.set_option(asio::socket_base::reuse_address(BindOpts.bReuse_Address), error_code);
	if (error_code) {
		FScopeLock lock(&mutex_error);
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
		OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	net.acceptor.listen(max_connections, error_code);
	if (error_code) {
		FScopeLock lock(&mutex_error);
		OnError.Broadcast(FErrorCode(error_code));
		return false;
	}

	asio::post(thread_pool(), [&]{ run_context_thread(); });
	return true;
}

void UTCPServerSsl::Close() {
	is_closing.Store(true);
	if (net.acceptor.is_open()) {
		FScopeLock lock(&mutex_error);
		net.acceptor.close(error_code);
		if (error_code)
			AsyncTask(ENamedThreads::GameThread, [&]() {
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
	AsyncTask(ENamedThreads::GameThread, [&]() {
		OnClose.Broadcast();
	});
	is_closing.Store(false);
}

void UTCPServerSsl::run_context_thread() {
	FScopeLock lock(&mutex_error);
	error_code.clear();
	UTCPRemoteSsl* client_socket = NewObject<UTCPRemoteSsl>();
	client_socket->Construct(net.context, net.ssl_context);
	net.acceptor.async_accept(client_socket->get_socket().lowest_layer(),
								[&, client_socket](const asio::error_code &ec) {
									accept(ec, client_socket);
								});
	net.context.run();
	if (!is_closing.Load())
		Close();
}

void UTCPServerSsl::accept(const asio::error_code& error, UTCPRemoteSsl* client) {
	if (error) {
		FScopeLock lock(&mutex_error);
		error_code = error;
		if (!is_closing.Load())
			client->Close();
		if (net.acceptor.is_open()) {
			UTCPRemoteSsl* client_socket = NewObject<UTCPRemoteSsl>();
			client_socket->Construct(net.context, net.ssl_context);
			net.acceptor.async_accept(client_socket->get_socket().lowest_layer(),
										[&, client_socket](const asio::error_code &ec) {
											accept(ec, client_socket);
										});
		}
		return;
	}
	if (net.ssl_clients.Num() < max_connections) {
		client->connect();
		net.ssl_clients.Add(client);
		client->on_close = [&, client]() { net.ssl_clients.Remove(client); };
		AsyncTask(ENamedThreads::GameThread, [&]() {
			OnClientAccepted.Broadcast(client);
		});		
	} else {
		FScopeLock lock(&mutex_error);
		if (!is_closing)
			client->Close();
	}
	if (net.acceptor.is_open()) {
		UTCPRemoteSsl* client_socket = NewObject<UTCPRemoteSsl>();
		client_socket->Construct(net.context, net.ssl_context);
		net.acceptor.async_accept(client_socket->get_socket().lowest_layer(),
									[&, client_socket](const asio::error_code &ec) {
										accept(ec, client_socket);
									});
	}
}
