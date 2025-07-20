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

        bool is_open() const { return net.acceptor.is_open(); }

        tcp::endpoint local_endpoint() const { return net.acceptor.local_endpoint(); }

        const std::set<std::shared_ptr<http_remote_c>> &clients() const { return net.clients; }

        const asio::error_code &get_error_code() const { return error_code; }

        void set_max_connections(const int max_connections) {
            this->max_connections = max_connections;
        }

        int get_max_connections() const { return max_connections; }

        void all(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            all_cb.insert_or_assign(path, callback);
        }
        void get(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            get_cb.insert_or_assign(path, callback);
        }
        void post(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            post_cb.insert_or_assign(path, callback);
        }
        void put(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            put_cb.insert_or_assign(path, callback);
        }
        void del(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            del_cb.insert_or_assign(path, callback);
        }
        void head(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            head_cb.insert_or_assign(path, callback);
        }
        void options(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            options_cb.insert_or_assign(path, callback);
        }
        void path(const std::string &path, const std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)> &callback) {
            path_cb.insert_or_assign(path, callback);
        }

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

        std::function<void()> on_close;
        std::function<void(const asio::error_code &)> on_error;
    private:
        std::mutex mutex_io;
        std::mutex mutex_error;
        std::atomic<bool> is_closing = false;
        http_server_t net;
        asio::error_code error_code;
        int max_connections = 2147483647;

        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> all_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> get_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> post_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> put_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> del_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> head_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> options_cb;
        std::map<std::string, std::function<void(const http_request_t &, const std::shared_ptr<http_remote_c> &)>> path_cb;

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            error_code.clear();
            std::shared_ptr<http_remote_c> client_socket = std::make_shared<http_remote_c>(net.context);
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
                    std::shared_ptr<http_remote_c> client_socket = std::make_shared<http_remote_c>(net.context);
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
                std::shared_ptr<http_remote_c> client_socket = std::make_shared<http_remote_c>(net.context);
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
                    if (path_cb.find(request.path) != path_cb.end()) {
                        if (path_cb[request.path]) {
                            path_cb[request.path](request, client);
                        }
                    }
                    break;
                case CONNECT:
                    if (path_cb.find(request.path) != path_cb.end()) {
                        if (path_cb[request.path]) {
                            path_cb[request.path](request, client);
                        }
                    }
                    break;
                case TRACE:
                    if (path_cb.find(request.path) != path_cb.end()) {
                        if (path_cb[request.path]) {
                            path_cb[request.path](request, client);
                        }
                    }
                    break;
            }
        }
    };
}
