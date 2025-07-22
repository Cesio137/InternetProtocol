#pragma once

#include "ip/net/common.hpp"
#include "ip/http/httpremote.hpp"

namespace internetprotocol {
    struct http_server_t {
        http_server_t(): acceptor(context) {
        }

        asio::io_context context;
        tcp::acceptor acceptor;
        std::set<std::shared_ptr<http_remote_c> > clients;
    };

    class http_server_c {
    public:
        http_server_c() {}

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * http_client_c client;
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
         *     http_response_t &res = response->get_response();
         *     res.body = "hello, world!";
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
         * http_server_c server;
         * // Limit the server to handle at most 100 concurrent clients
         * server.set_max_connections(100);
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
         * http_server_c server;
         * // Get the current connection limit
         * int max_conn = server.get_max_connections();
         * @endcode
         */
        int get_max_connections() const { return max_connections; }

        /**
         * Set the idle timeout for the connection in seconds.
         * A value of 0 disables the idle timeout.
         *
         * @param timeout The idle timeout duration in seconds. Default is 0.
         *
         * @par
         * @code
         * http_server_c server;
         * // Set idle timeout seconds
         * server.set_idle_timeout(5);
         * @endcode
         */
        void set_idle_timeout(const uint16_t timeout = 0) {
            iddle_timeout = timeout;
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
         * http_server_c server;
         * // Get idle timeout in seconds
         * int timeout = server.get_idle_timeout();
         * @endcode
         */
        uint16_t get_idle_timeout() const { return iddle_timeout; }

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
         *      res.body = "Your remote port is: " + std::to_string(response->remote_endpoint().port());
         *      res.body = "hello, world!";
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
         *      res.body = "Your remote port is: " + std::to_string(response->remote_endpoint().port());
         *      res.body = "hello, world!";
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
         *      res.body = "Your remote port is: " + std::to_string(response->remote_endpoint().port());
         *      res.body = "hello, world!";
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
         *      res.body = "Your remote port is: " + std::to_string(response->remote_endpoint().port());
         *      res.body = "hello, world!";
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
         *      res.body = "Your remote port is: " + std::to_string(response->remote_endpoint().port());
         *      res.body = "hello, world!";
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
         *      res.body = "Your remote port is: " + std::to_string(response->remote_endpoint().port());
         *      res.body = "hello, world!";
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
         *      res.body = "Your remote port is: " + std::to_string(response->remote_endpoint().port());
         *      res.body = "hello, world!";
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
/**/
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
         * http_server_c server;
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
        http_server_t net;
        asio::error_code error_code;
        int max_connections = 2147483647;
        uint16_t iddle_timeout = 0;

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
            if (net.clients.size() < max_connections) {
                client->on_request = [&, client](const http_request_t &request) {
                    read_cb(request, client);
                };
                client->on_close = [&, client]() { net.clients.erase(client); };
                client->connect();
                net.clients.insert(client);
                client->on_close = [&, client]() { net.clients.erase(client); };
            } else {
                std::lock_guard guard(mutex_error);
                if (!is_closing)
                    client->close();
            }
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
                case CONNECT:
                    if (patch_cb.find(request.path) != patch_cb.end()) {
                        if (patch_cb[request.path]) {
                            patch_cb[request.path](request, client);
                        }
                    }
                    break;
                case TRACE:
                    if (patch_cb.find(request.path) != patch_cb.end()) {
                        if (patch_cb[request.path]) {
                            patch_cb[request.path](request, client);
                        }
                    }
                    break;
            }
        }
    };
}
