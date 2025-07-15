#pragma once

#include <thread>

#include "ip/net/common.hpp"

namespace ip {
    class tcp_server_c {
    public:
        tcp_server_c() {}
        ~tcp_server_c() {
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
         * Get a reference to a std::set of connected clients.
         *
         * @par Example
         * @code
         * tcp_server_c server;
         * asio::ip::tcp::endpoint ep = server.local_endpoint();
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

        void set_max_connections(const int max_connections) {
            this->max_connections = max_connections;
        }
        int get_max_connections() const { return max_connections; }


        bool open(const udp_bind_options_t &bind_opts = { "", 8080, v4, true }) {
            if (net.acceptor.is_open())
                return false;

            tcp::endpoint endpoint(resolver.protocol == v4
                                                 ? tcp::v4()
                                                 : tcp::v6(),
                                                 resolver.port);
            error_code = asio::error_code();
            net.acceptor.open(resolver.protocol == v4
                                  ? tcp::v4()
                                  : tcp::v6(),
                                  error_code);
            if (error_code && on_error) {
                std::lock_guard guard(mutex_error);
                on_error(error_code);
                return false;
            }
            net.acceptor.set_option(asio::socket_base::reuse_address(true), error_code);
            if (error_code && on_error) {
                std::lock_guard guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }
            net.acceptor.bind(endpoint, error_code);
            if (error_code && on_error) {
                std::lock_guard guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }
            net.acceptor.listen(max_connections, error_code);
            if (error_code && on_error) {
                std::lock_guard guard(mutex_error);
                if (on_error) on_error(error_code);
                return false;
            }

            asio::post(thread_pool, [&]{ run_context_thread(); });
            return true;
        }

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
            }
            net.context.stop();
            if (!net.clients.empty())
                net.clients.clear();

            net.context.restart();
            net.acceptor = tcp::acceptor(net.context);
            if (on_close)
                on_close();
            is_closing.store(false);
        }

        /*EVENTS*/
        std::function<void(const std::shared_ptr<tcp_remote_c> &)> on_client_accepted;
        //std::function<void(const size_t, const size_t)> on_bytes_transfered;
        //std::function<void(const asio::error_code &, const std::shared_ptr<tcp_remote_c> &)> on_message_sent;
        //std::function<void(const std::vector<uint8_t> &, const std::shared_ptr<tcp_remote_c> &)> on_message_received;
        //std::function<void(const asio::error_code &, const std::shared_ptr<tcp::socket> &)> on_socket_disconnected;
        std::function<void()> on_close;
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
}
