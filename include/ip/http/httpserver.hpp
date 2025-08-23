/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
*/

#pragma once

#include "ip/net/common.hpp"
#include "ip/http/httpremote.hpp"

namespace internetprotocol {
    class http_server_c {
    public:
        http_server_c() {}
        ~http_server_c() {
            if (net.acceptor.is_open())
                close();
        }

        /**
         * Set/Get the idle timeout for the connection in seconds.
         * A value of 0 disables the idle timeout.
         *
         * @param timeout The idle timeout duration in seconds. Default is 0.
         *
         * @par
         * @code
         * http_server_c server;
         * // Set idle timeout seconds
         * server.idle_timeout = 5;
         * @endcode
         */
        uint16_t iddle_timeout = 0;

        /**
         * Set/Get the maximum number of simultaneous client connections the server will accept in queue.
         *
         * This method configures the maximum number of concurrent client connections
         * that the server will maintain in queue. When this limit is reached, any new connection
         * attempts will be rejected automatically. The default value is INT_MAX (2147483647).
         *
         * @param max_connections The maximum number of concurrent connections to allow.
         *                        Must be a positive integer.
         *
         * @par Example
         * @code
         * http_server_c server;
         * // Limit the server to handle at most 100 concurrent clients
         * server.backlog = 100;
         * @endcode
         */
        int backlog = 2147483647;

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * http_server_c client;
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return net.acceptor.is_open(); }


        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * http_server_c server;
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
         * http_server_c server;
         * server.open();
         *
         * // Check the number of connected clients
         * size_t num_clients = server.clients().size();
         *
         * // Iterate over all connected clients
         * for(const auto& client : server.clients()) {
         *     // Perform some operation with each client
         *     http_response_t &headers = response->headers();
         *     headers.body = "hello, world!";
         *     client->write();
         * }
         * @endcode
         */
        const std::set<std::shared_ptr<http_remote_c>> &clients() const { return net.clients; }

        /**
         * Return a const ref of the latest error code returned by asio.
         *
         * @par Example
         * @code
         * http_server_c server;
         * asio::error_code ec = server.get_error_code();
         * @endcode
         */
        const asio::error_code &get_error_code() const { return error_code; }

        /**
         * Create a callback to receive requests of any methods for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_c server;
         * server.all("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_c> &response) {
         *      res.body = "Your remote port is: " + std::to_string(response->remote_endpoint().port());
         *      res.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void all(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            all_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of get method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_c server;
         * server.get("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void get(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            get_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of post method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_c server;
         * server.post("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void post(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            post_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of put method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_c server;
         * server.put("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void put(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            put_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of delete method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_c server;
         * server.del("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void del(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            del_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of head method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_c server;
         * server.head("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void head(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            head_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of options method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_c server;
         * server.options("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void options(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            options_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of patch method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_c server;
         * server.patch("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void patch(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            patch_cb.insert_or_assign(path, callback);
        }

        /**
         * Opens the HTTP server and starts listening for incoming connections.
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
         * http_server_c server;
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

            net.acceptor.listen(backlog, error_code);
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
         * http_server_c server;
         * server.close(false);
         * @endcode
         */
        void close() {
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
                        client->close();
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
         * Adds the listener function to 'on_close'.
         * This event will be triggered after acceptor socket has been closed and all client has been disconnected.
         *
         * @par Example
         * @code
         * http_server_c server;
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
         * http_server_c server;
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
        tcp_server_t<http_remote_c> net;
        asio::error_code error_code;

        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> all_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> get_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> post_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> put_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> del_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> head_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> options_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> patch_cb;

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            error_code.clear();
            std::shared_ptr<http_remote_c> client_socket = std::make_shared<http_remote_c>(net.context, iddle_timeout);
            net.acceptor.async_accept(client_socket->get_socket(),
                                        [&, client_socket](const asio::error_code &ec) {
                                            accept(ec, client_socket);
                                        });
            net.context.run();
            if (!is_closing.load())
                close();
        }

        void accept(const asio::error_code &error, const std::shared_ptr<http_remote_c> &client) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (!is_closing.load())
                    client->close();
                if (on_error) on_error(error_code);
                if (net.acceptor.is_open()) {
                    std::shared_ptr<http_remote_c> client_socket = std::make_shared<http_remote_c>(net.context, iddle_timeout);
                    net.acceptor.async_accept(client_socket->get_socket(),
                                                [&, client_socket](const asio::error_code &ec) {
                                                    accept(ec, client_socket);
                                                });
                }
                return;
            }
            client->on_request = [&, client](const http_request_t &request) {
                read_cb(request, client);
            };
            client->on_close = [&, client]() { net.clients.erase(client); };
            client->connect();
            net.clients.insert(client);
            if (net.acceptor.is_open()) {
                std::shared_ptr<http_remote_c> client_socket = std::make_shared<http_remote_c>(net.context, iddle_timeout);
                net.acceptor.async_accept(client_socket->get_socket(),
                                        [&, client_socket](const asio::error_code &ec) {
                                            accept(ec, client_socket);
                                        });
            }
        }

        void read_cb(const http_request_t &request, const std::shared_ptr<http_remote_c> &client) {
            if (all_cb.find(request.path) != all_cb.end()) {
                if (all_cb[request.path]) {
                    all_cb[request.path](request, client);
                }
            }
            switch (request.method) {
                case DEL:
                    if (del_cb.find(request.path) != del_cb.end()) {
                        if (del_cb[request.path]) {
                            del_cb[request.path](request, client);
                        }
                    }
                    break;
                case GET:
                    if (get_cb.find(request.path) != get_cb.end()) {
                        if (get_cb[request.path]) {
                            get_cb[request.path](request, client);
                        }
                    }
                    break;
                case HEAD:
                    if (head_cb.find(request.path) != head_cb.end()) {
                        if (head_cb[request.path]) {
                            head_cb[request.path](request, client);
                        }
                    }
                    break;
                case OPTIONS:
                    if (options_cb.find(request.path) != options_cb.end()) {
                        if (options_cb[request.path]) {
                            options_cb[request.path](request, client);
                        }
                    }
                    break;
                case POST:
                    if (post_cb.find(request.path) != post_cb.end()) {
                        if (post_cb[request.path]) {
                            post_cb[request.path](request, client);
                        }
                    }
                    break;
                case PUT:
                    if (put_cb.find(request.path) != put_cb.end()) {
                        if (put_cb[request.path]) {
                            put_cb[request.path](request, client);
                        }
                    }
                    break;
                case PATCH:
                    if (patch_cb.find(request.path) != patch_cb.end()) {
                        if (patch_cb[request.path]) {
                            patch_cb[request.path](request, client);
                        }
                    }
                    break;
            }
        }
    };

#ifdef ENABLE_SSL
    class http_server_ssl_c {
    public:
        http_server_ssl_c(const security_context_opts sec_opts = {}) {
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
        ~http_server_ssl_c() {
            if (net.acceptor.is_open())
                close();
        }

        /**
         * Set/Get the idle timeout for the connection in seconds.
         * A value of 0 disables the idle timeout.
         *
         * @param timeout The idle timeout duration in seconds. Default is 0.
         *
         * @par
         * @code
         * http_server_ssl_c server({});
         * // Set idle timeout seconds
         * server.idle_timeout = 5;
         * @endcode
         */
        uint16_t iddle_timeout = 0;

        /**
         * Set/Get the maximum number of simultaneous client connections the server will accept in queue.
         *
         * This method configures the maximum number of concurrent client connections
         * that the server will maintain in queue. When this limit is reached, any new connection
         * attempts will be rejected automatically. The default value is INT_MAX (2147483647).
         *
         * @param max_connections The maximum number of concurrent connections to allow.
         *                        Must be a positive integer.
         *
         * @par Example
         * @code
         * http_server_ssl_c server({});
         * // Limit the server to handle at most 100 concurrent clients
         * server.backlog = 100;
         * @endcode
         */
        int backlog = 2147483647;

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * http_server_ssl_c server;
         * bool is_open = server.is_open();
         * @endcode
         */
        bool is_open() const { return net.acceptor.is_open(); }


        /**
         * Get the local endpoint of the socket. Use this function only after open connection.
         *
         * @par Example
         * @code
         * http_server_ssl_c server;
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
         * http_server_ssl_c server;
         * server.open();
         *
         * // Check the number of connected clients
         * size_t num_clients = server.clients().size();
         *
         * // Iterate over all connected clients
         * for(const auto& client : server.clients()) {
         *     // Perform some operation with each client
         *     http_response_t &headers = response->headers();
         *     headers.body = "hello, world!";
         *     client->write();
         * }
         * @endcode
         */
        const std::set<std::shared_ptr<http_remote_ssl_c>> &clients() const { return net.ssl_clients; }

        /**
         * Return a const ref of the latest error code returned by asio.
         *
         * @par Example
         * @code
         * http_server_ssl_c server;
         * asio::error_code ec = server.get_error_code();
         * @endcode
         */
        const asio::error_code &get_error_code() const { return error_code; }

        /**
         * Create a callback to receive requests of any methods for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_ssl_c server;
         * server.all("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_ssl_c> &response) {
         *      res.body = "Your remote port is: " + std::to_string(response->remote_endpoint().port());
         *      res.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void all(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)> &callback) {
            all_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of get method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_ssl_c server;
         * server.get("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_ssl_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void get(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)> &callback) {
            get_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of post method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_ssl_c server;
         * server.post("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_ssl_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void post(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)> &callback) {
            post_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of put method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_ssl_c server;
         * server.put("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_ssl_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void put(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)> &callback) {
            put_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of delete method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_ssl_c server;
         * server.del("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_ssl_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void del(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)> &callback) {
            del_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of head method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_ssl_c server;
         * server.head("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_ssl_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void head(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)> &callback) {
            head_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of options method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_ssl_c server;
         * server.options("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_ssl_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void options(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)> &callback) {
            options_cb.insert_or_assign(path, callback);
        }

        /**
         * Create a callback to receive requests of patch method for a specific path.
         *
         * @param path URL path from request.
         * @param callback This callback is triggered when a request has been received.
         *
         * @par Example
         * @code
         * http_server_ssl_c server;
         * server.patch("/", [&](const http_request_t &request, const std::shared_ptr<http_remote_ssl_c> &response) {
         *      http_response_t &headers = response->headers();
         *      headers.body = "hello, world!";
         *      res.write();
         * };
         * @endcode
         */
        void patch(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)> &callback) {
            patch_cb.insert_or_assign(path, callback);
        }

        /**
         * Opens the HTTP server and starts listening for incoming connections.
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
         * http_server_ssl_c server;
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

            net.acceptor.listen(backlog, error_code);
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
         * http_server_ssl_c server;
         * server.close(false);
         * @endcode
         */
        void close() {
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
                        client->close();
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
         * Adds the listener function to 'on_close'.
         * This event will be triggered after acceptor socket has been closed and all client has been disconnected.
         *
         * @par Example
         * @code
         * http_server_ssl_c server;
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
         * http_server_ssl_c server;
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
        tcp_server_ssl_t<http_remote_ssl_c> net;
        asio::error_code error_code;

        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)>> all_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)>> get_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)>> post_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)>> put_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)>> del_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)>> head_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)>> options_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_ssl_c> &)>> patch_cb;

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            error_code.clear();
            std::shared_ptr<http_remote_ssl_c> client_socket = std::make_shared<http_remote_ssl_c>(net.context, net.ssl_context, iddle_timeout);
            net.acceptor.async_accept(client_socket->get_socket().lowest_layer(),
                                        [&, client_socket](const asio::error_code &ec) {
                                            accept(ec, client_socket);
                                        });
            net.context.run();
            if (!is_closing.load())
                close();
        }

        void accept(const asio::error_code &error, const std::shared_ptr<http_remote_ssl_c> &client) {
            if (error) {
                std::lock_guard guard(mutex_error);
                error_code = error;
                if (!is_closing.load())
                    client->close();
                if (on_error) on_error(error_code);
                if (net.acceptor.is_open()) {
                    std::shared_ptr<http_remote_ssl_c> client_socket = std::make_shared<http_remote_ssl_c>(net.context, net.ssl_context, iddle_timeout);
                    net.acceptor.async_accept(client_socket->get_socket().lowest_layer(),
                                                [&, client_socket](const asio::error_code &ec) {
                                                    accept(ec, client_socket);
                                                });
                }
                return;
            }
            client->on_request = [&, client](const http_request_t &request) {
                read_cb(request, client);
            };
            client->on_close = [&, client]() { net.ssl_clients.erase(client); };
            client->connect();
            net.ssl_clients.insert(client);
            if (net.acceptor.is_open()) {
                std::shared_ptr<http_remote_ssl_c> client_socket = std::make_shared<http_remote_ssl_c>(net.context, net.ssl_context, iddle_timeout);
                net.acceptor.async_accept(client_socket->get_socket().lowest_layer(),
                                        [&, client_socket](const asio::error_code &ec) {
                                            accept(ec, client_socket);
                                        });
            }
        }

        void read_cb(const http_request_t &request, const std::shared_ptr<http_remote_ssl_c> &client) {
            if (all_cb.find(request.path) != all_cb.end()) {
                if (all_cb[request.path]) {
                    all_cb[request.path](request, client);
                }
            }
            switch (request.method) {
                case DEL:
                    if (del_cb.find(request.path) != del_cb.end()) {
                        if (del_cb[request.path]) {
                            del_cb[request.path](request, client);
                        }
                    }
                    break;
                case GET:
                    if (get_cb.find(request.path) != get_cb.end()) {
                        if (get_cb[request.path]) {
                            get_cb[request.path](request, client);
                        }
                    }
                    break;
                case HEAD:
                    if (head_cb.find(request.path) != head_cb.end()) {
                        if (head_cb[request.path]) {
                            head_cb[request.path](request, client);
                        }
                    }
                    break;
                case OPTIONS:
                    if (options_cb.find(request.path) != options_cb.end()) {
                        if (options_cb[request.path]) {
                            options_cb[request.path](request, client);
                        }
                    }
                    break;
                case POST:
                    if (post_cb.find(request.path) != post_cb.end()) {
                        if (post_cb[request.path]) {
                            post_cb[request.path](request, client);
                        }
                    }
                    break;
                case PUT:
                    if (put_cb.find(request.path) != put_cb.end()) {
                        if (put_cb[request.path]) {
                            put_cb[request.path](request, client);
                        }
                    }
                    break;
                case PATCH:
                    if (patch_cb.find(request.path) != patch_cb.end()) {
                        if (patch_cb[request.path]) {
                            patch_cb[request.path](request, client);
                        }
                    }
                    break;
            }
        }
    };
#endif
}
