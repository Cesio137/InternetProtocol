#pragma once

#include "ip/net/common.hpp"
#include "ip/tcp/tcpremote.hpp"

namespace internetprotocol {
    struct tcp_server_t {
        tcp_server_t(): acceptor(context) {
        }

        asio::io_context context;
        tcp::acceptor acceptor;
        std::set<std::shared_ptr<tcp_remote_c> > clients;
    };

    class tcp_server_c {
    public:
        tcp_server_c() {}

        virtual ~tcp_server_c() {
            if (net.acceptor.is_open() || net.clients.size() > 0) {
                close();
            }
        }

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         * bool is_open = server.is_open();
         * @endcode
         */
        bool is_open() const { return net.acceptor.is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         * asio::ip::tcp::endpoint ep = server.local_endpoint();
         * @endcode
         */
        tcp::endpoint local_endpoint() const { return net.acceptor.local_endpoint(); }

        /**
         * Gets a constant reference to the set of clients connected to the server.
         *
         * This method returns a std::set containing shared pointers to all tcp_remote_c
         * clients currently connected to the server. Useful for monitoring active
         * connections or performing operations on all connected clients.
         *
         * @return A constant reference to the std::set of connected clients.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         * server.open();
         *
         * // Check the number of connected clients
         * size_t num_clients = server.clients().size();
         *
         * // Iterate over all connected clients
         * for(const auto& client : server.clients()) {
         *     // Perform some operation with each client
         *     client->send("Message to everyone");
         * }
         * @endcode
         */
        const std::set<std::shared_ptr<tcp_remote_c>> &clients() const { return net.clients; }

        /**
         * Return a const ref of the latest error code returned by asio.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         * asio::error_code ec = server.get_error_code();
         * @endcode
         */
        const asio::error_code &get_error_code() const { return error_code; }

        /**
         * Sets the maximum number of simultaneous client connections the server will accept.
         *
         * This method configures the maximum number of concurrent client connections
         * that the server will maintain. When this limit is reached, any new connection
         * attempts will be rejected automatically. The default value is INT_MAX (2147483647).
         *
         * @param max_connections The maximum number of concurrent connections to allow.
         *                        Must be a positive integer.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         * // Limit the server to handle at most 100 concurrent clients
         * server.set_max_connections(100);
         * server.open({"", 8080});
         * @endcode
         */
        void set_max_connections(const int max_connections) {
            this->max_connections = max_connections;
        }

        /**
         * Gets the current maximum connection limit for this server.
         *
         * Returns the maximum number of simultaneous client connections
         * that this server is configured to accept.
         *
         * @return The current maximum connection limit as an integer.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         * // Get the current connection limit
         * int max_conn = server.get_max_connections();
         * @endcode
         */
        int get_max_connections() const { return max_connections; }

        /**
         * Opens the TCP server and starts listening for incoming connections.
         *
         * This method initializes the server socket, binds it to the specified address and port,
         * and begins listening for client connections. If the server is already open, the method
         * returns false without making any changes.
         *
         * If any operation fails, the error handler (on_error) will be called if set,
         * and the method will return false.
         *
         * @param bind_opts An struct specifying protocol parameters to be used.
         * If 'port' is not specified or is 0, the operating system will attempt to bind to a random port.
         * If 'address' is not specified, the operating system will attempt to listen on all addresses.
         *
         * @return true if the server was successfully opened and is now listening;
         *         false if the server is already open or if any error occurred during setup.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         *
         * server.open({"", 8080, v4, true})
         * @endcode
         */
        bool open(const server_bind_options_t &bind_opts = { "", 8080, v4, true }) {
            if (net.acceptor.is_open())
                return false;

            net.acceptor.open(bind_opts.protocol == v4
                                  ? tcp::v4()
                                  : tcp::v6(),
                                  error_code);

            if (error_code && on_error) {
                std::lock_guard guard(mutex_error);
                on_error(error_code);
                return false;
            }

            net.acceptor.set_option(asio::socket_base::reuse_address(bind_opts.reuse_address), error_code);
            if (error_code && on_error) {
                std::lock_guard guard(mutex_error);
                on_error(error_code);
                return false;
            }

            const tcp::endpoint endpoint = bind_opts.address.empty() ?
                                        tcp::endpoint(bind_opts.protocol == v4
                                                        ? tcp::v4()
                                                        : tcp::v6(),
                                                        bind_opts.port) :
                                        tcp::endpoint(make_address(bind_opts.address), bind_opts.port);

            net.acceptor.bind(endpoint, error_code);
            if (error_code && on_error) {
                std::lock_guard guard(mutex_error);
                on_error(error_code);
                return false;
            }

            net.acceptor.listen(max_connections, error_code);
            if (error_code && on_error) {
                std::lock_guard guard(mutex_error);
                on_error(error_code);
                return false;
            }

            asio::post(thread_pool, [&]{ run_context_thread(); });
            return true;
        }

        /**
         * Close the underlying socket and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @param force Cancel all asynchronous operations associated with the client sockets if true.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         * server.close(false);
         * @endcode
         */
        void close(const bool force = false) {
            is_closing.store(true);
            if (net.acceptor.is_open()) {
                std::lock_guard guard(mutex_error);
                net.acceptor.close(error_code);
                if (on_error) on_error(error_code);
            }
            if (!net.clients.empty()) {
                std::lock_guard guard(mutex_error);
                for (const auto &client : net.clients) {
                    if (client)
                        client->close(force);
                }
                net.clients.clear();
            }
            net.context.stop();
            net.context.restart();
            net.acceptor = tcp::acceptor(net.context);
            if (on_close)
                on_close();
            is_closing.store(false);
        }

        /**
         * Adds the listener function to 'on_listening'.
         * This event will be triggered when socket start to listening.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         * server.on_listening = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_listening;

        /**
         * Adds the listener function to 'on_client_accepted'.
         * This event will be triggered when new client connect.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         * server.on_client_accepted = [&](const std::shared_ptr<tcp_remote_c> &remote) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const std::shared_ptr<tcp_remote_c> &)> on_client_accepted;

        /**
         * Adds the listener function to 'on_close'.
         * This event will be triggered after acceptor socket has been closed and all client has been disconnected.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         * server.on_close = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_close;

        /**
         * Adds the listener function to 'on_error'.
         * This event will be triggered when any error has been returned by asio.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         * server.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_error;
        std::atomic<bool> is_closing = false;
        tcp_server_t net;
        asio::error_code error_code;
        int max_connections = 2147483647;

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            error_code.clear();
            std::shared_ptr<tcp_remote_c> client_socket = std::make_shared<tcp_remote_c>(net.context);
            net.acceptor.async_accept(client_socket->get_socket(),
                                        [&, client_socket](const asio::error_code &ec) {
                                            accept(ec, client_socket);
                                        });
            net.context.run();
            if (!is_closing.load())
                close();
        }

        void accept(const asio::error_code &error, const std::shared_ptr<tcp_remote_c> &client) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (!is_closing.load())
                    client->close();
                if (net.acceptor.is_open()) {
                    std::shared_ptr<tcp_remote_c> client_socket = std::make_shared<tcp_remote_c>(net.context);
                    net.acceptor.async_accept(client_socket->get_socket(),
                                                [&, client_socket](const asio::error_code &ec) {
                                                    accept(ec, client_socket);
                                                });
                }
                return;
            }
            if (net.clients.size() < max_connections) {
                client->connect();
                net.clients.insert(client);
                client->on_close = [&, client]() { net.clients.erase(client); };

                if (on_client_accepted)
                    on_client_accepted(client);
            } else {
                std::lock_guard guard(mutex_error);
                if (!is_closing)
                    client->close();
            }
            if (net.acceptor.is_open()) {
                std::shared_ptr<tcp_remote_c> client_socket = std::make_shared<tcp_remote_c>(net.context);
                net.acceptor.async_accept(client_socket->get_socket(),
                                        [&, client_socket](const asio::error_code &ec) {
                                            accept(ec, client_socket);
                                        });
            }
        }
    };
#ifdef ENABLE_SSL
    struct tcp_server_ssl_t {
        tcp_server_ssl_t(): acceptor(context), ssl_context(asio::ssl::context::tlsv13_server) {
        }

        asio::io_context context;
        asio::ssl::context ssl_context;
        tcp::acceptor acceptor;
        std::set<std::shared_ptr<tcp_remote_ssl_c> > ssl_clients;
    };

    class tcp_server_ssl_c {
    public:
        tcp_server_ssl_c(const security_context_opts sec_opts = {}) {
            if (!sec_opts.private_key.empty()) {
                const asio::const_buffer buffer(sec_opts.private_key.data(), sec_opts.private_key.size());
                net.ssl_context.use_private_key(buffer, sec_opts.file_format);
            }

            if (!sec_opts.cert.empty()) {
                const asio::const_buffer buffer(sec_opts.cert.data(), sec_opts.cert.size());
                net.ssl_context.use_certificate(buffer, sec_opts.file_format);
            }

            if (!sec_opts.cert_chain.empty()) {
                const asio::const_buffer buffer(sec_opts.cert_chain.data(), sec_opts.cert_chain.size());
                net.ssl_context.use_certificate_chain(buffer);
            }

            if (!sec_opts.rsa_private_key.empty()) {
                const asio::const_buffer buffer(sec_opts.rsa_private_key.data(), sec_opts.rsa_private_key.size());
                net.ssl_context.use_rsa_private_key(buffer, sec_opts.file_format);
            }

            if (!sec_opts.host_name_verification.empty()) {
                net.ssl_context.set_verify_callback(asio::ssl::host_name_verification(sec_opts.host_name_verification));
            }

            switch (sec_opts.verify_mode) {
                case none:
                    net.ssl_context.set_verify_mode(asio::ssl::verify_none);
                    break;
                case verify_peer:
                    net.ssl_context.set_verify_mode(asio::ssl::verify_peer);
                    break;
                case verify_fail_if_no_peer_cert:
                    net.ssl_context.set_verify_mode(asio::ssl::verify_fail_if_no_peer_cert);
                    break;
                default:
                    break;
            }
        }

        ~tcp_server_ssl_c() {}

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * tcp_server_ssl_c server({});
         * bool is_open = server.is_open();
         * @endcode
         */
        bool is_open() const { return net.acceptor.is_open(); }

        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * tcp_server_ssl_c server({});
         * asio::ip::tcp::endpoint ep = server.local_endpoint();
         * @endcode
         */
        tcp::endpoint local_endpoint() const { return net.acceptor.local_endpoint(); }

        /**
         * Gets a constant reference to the set of clients connected to the server.
         *
         * This method returns a std::set containing shared pointers to all tcp_remote_c
         * clients currently connected to the server. Useful for monitoring active
         * connections or performing operations on all connected clients.
         *
         * @return A constant reference to the std::set of connected clients.
         *
         * @par Example
         * @code
         * tcp_server_ssl_c server({});
         * server.open();
         *
         * // Check the number of connected clients
         * size_t num_clients = server.clients().size();
         *
         * // Iterate over all connected clients
         * for(const auto& client : server.clients()) {
         *     // Perform some operation with each client
         *     client->send("Message to everyone");
         * }
         * @endcode
         */
        const std::set<std::shared_ptr<tcp_remote_ssl_c>> &clients() const { return net.ssl_clients; }

        /**
         * Return a const ref of the latest error code returned by asio.
         *
         * @par Example
         * @code
         * tcp_server_ssl_c server({});
         * asio::error_code ec = server.get_error_code();
         * @endcode
         */
        const asio::error_code &get_error_code() const { return error_code; }

        /**
         * Sets the maximum number of simultaneous client connections the server will accept.
         *
         * This method configures the maximum number of concurrent client connections
         * that the server will maintain. When this limit is reached, any new connection
         * attempts will be rejected automatically. The default value is INT_MAX (2147483647).
         *
         * @param max_connections The maximum number of concurrent connections to allow.
         *                        Must be a positive integer.
         *
         * @par Example
         * @code
         * tcp_server_ssl_c server({});
         * // Limit the server to handle at most 100 concurrent clients
         * server.set_max_connections(100);
         * server.open({"", 8080});
         * @endcode
         */
        void set_max_connections(const int max_connections) {
            this->max_connections = max_connections;
        }

        /**
         * Gets the current maximum connection limit for this server.
         *
         * Returns the maximum number of simultaneous client connections
         * that this server is configured to accept.
         *
         * @return The current maximum connection limit as an integer.
         *
         * @par Example
         * @code
         * tcp_server_ssl_c server({});
         * // Get the current connection limit
         * int max_conn = server.get_max_connections();
         * @endcode
         */
        int get_max_connections() const { return max_connections; }

        /**
         * Opens the TCP server and starts listening for incoming connections.
         *
         * This method initializes the server socket, binds it to the specified address and port,
         * and begins listening for client connections. If the server is already open, the method
         * returns false without making any changes.
         *
         * If any operation fails, the error handler (on_error) will be called if set,
         * and the method will return false.
         *
         * @param bind_opts An struct specifying protocol parameters to be used.
         * If 'port' is not specified or is 0, the operating system will attempt to bind to a random port.
         * If 'address' is not specified, the operating system will attempt to listen on all addresses.
         *
         * @return true if the server was successfully opened and is now listening;
         *         false if the server is already open or if any error occurred during setup.
         *
         * @par Example
         * @code
         * tcp_server_ssl_c server({});
         *
         * server.open({"", 8080, v4, true})
         * @endcode
         */
        bool open(const server_bind_options_t &bind_opts = { "", 8080, v4, true }) {
            if (net.acceptor.is_open())
                return false;

            net.acceptor.open(bind_opts.protocol == v4
                                  ? tcp::v4()
                                  : tcp::v6(),
                                  error_code);

            if (error_code && on_error) {
                std::lock_guard guard(mutex_error);
                on_error(error_code);
                return false;
            }

            net.acceptor.set_option(asio::socket_base::reuse_address(bind_opts.reuse_address), error_code);
            if (error_code && on_error) {
                std::lock_guard guard(mutex_error);
                on_error(error_code);
                return false;
            }

            const tcp::endpoint endpoint = bind_opts.address.empty() ?
                                        tcp::endpoint(bind_opts.protocol == v4
                                                        ? tcp::v4()
                                                        : tcp::v6(),
                                                        bind_opts.port) :
                                        tcp::endpoint(make_address(bind_opts.address), bind_opts.port);

            net.acceptor.bind(endpoint, error_code);
            if (error_code && on_error) {
                std::lock_guard guard(mutex_error);
                on_error(error_code);
                return false;
            }

            net.acceptor.listen(max_connections, error_code);
            if (error_code && on_error) {
                std::lock_guard guard(mutex_error);
                on_error(error_code);
                return false;
            }

            asio::post(thread_pool, [&]{ run_context_thread(); });
            return true;
        }

        /**
         * Close the underlying socket and stop listening for data on it. 'on_close' event will be triggered.
         *
         * @param force Cancel all asynchronous operations associated with the client sockets if true.
         *
         * @par Example
         * @code
         * tcp_server_ssl_c server({});
         * server.close(false);
         * @endcode
         */
        void close(const bool force = false) {
            is_closing.store(true);
            if (net.acceptor.is_open()) {
                std::lock_guard guard(mutex_error);
                net.acceptor.close(error_code);
                if (on_error) on_error(error_code);
            }
            if (!net.ssl_clients.empty()) {
                std::lock_guard guard(mutex_error);
                for (const auto &client : net.ssl_clients) {
                    if (client)
                        client->close(force);
                }
                net.ssl_clients.clear();
            }
            net.context.stop();
            net.context.restart();
            net.acceptor = tcp::acceptor(net.context);
            if (on_close)
                on_close();
            is_closing.store(false);
        }

        /**
         * Adds the listener function to 'on_listening'.
         * This event will be triggered when socket start to listening.
         *
         * @par Example
         * @code
         * tcp_server_ssl_c server({});
         * server.on_listening = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_listening;

        /**
         * Adds the listener function to 'on_client_accepted'.
         * This event will be triggered when new client connect.
         *
         * @par Example
         * @code
         * tcp_server_ssl_c server({});
         * server.on_client_accepted = [&](const std::shared_ptr<tcp_remote_ssl_c> &remote) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const std::shared_ptr<tcp_remote_ssl_c> &)> on_client_accepted;

        /**
         * Adds the listener function to 'on_close'.
         * This event will be triggered after acceptor socket has been closed and all client has been disconnected.
         *
         * @par Example
         * @code
         * tcp_server_ssl_c server({});
         * server.on_close = [&]() {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void()> on_close;

        /**
         * Adds the listener function to 'on_error'.
         * This event will be triggered when any error has been returned by asio.
         *
         * @par Example
         * @code
         * tcp_server_ssl_c server({});
         * server.on_error = [&](const asio::error_code &ec) {
         *      // your code...
         * };
         * @endcode
         */
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_io;
        std::mutex mutex_error;
        std::atomic<bool> is_closing = false;
        tcp_server_ssl_t net;
        asio::error_code error_code;
        int max_connections = 2147483647;

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            error_code.clear();
            std::shared_ptr<tcp_remote_ssl_c> client_socket = std::make_shared<tcp_remote_ssl_c>(net.context, net.ssl_context);
            net.acceptor.async_accept(client_socket->get_socket().lowest_layer(),
                                        [&, client_socket](const asio::error_code &ec) {
                                            accept(ec, client_socket);
                                        });
            net.context.run();
            if (!is_closing.load())
                close();
        }

        void accept(const asio::error_code &error, const std::shared_ptr<tcp_remote_ssl_c> &client) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (!is_closing.load())
                    client->close();
                if (net.acceptor.is_open()) {
                    std::shared_ptr<tcp_remote_ssl_c> client_socket = std::make_shared<tcp_remote_ssl_c>(net.context, net.ssl_context);
                    net.acceptor.async_accept(client_socket->get_socket().lowest_layer(),
                                                [&, client_socket](const asio::error_code &ec) {
                                                    accept(ec, client_socket);
                                                });
                }
                return;
            }
            if (net.ssl_clients.size() < max_connections) {
                client->connect();
                net.ssl_clients.insert(client);
                client->on_close = [&, client]() { net.ssl_clients.erase(client); };

                if (on_client_accepted)
                    on_client_accepted(client);
            } else {
                std::lock_guard guard(mutex_error);
                if (!is_closing)
                    client->close();
            }
            if (net.acceptor.is_open()) {
                std::shared_ptr<tcp_remote_ssl_c> client_socket = std::make_shared<tcp_remote_ssl_c>(net.context, net.ssl_context);
                net.acceptor.async_accept(client_socket->get_socket().lowest_layer(),
                                            [&, client_socket](const asio::error_code &ec) {
                                                accept(ec, client_socket);
                                            });
            }
        }
    };
#endif
}
