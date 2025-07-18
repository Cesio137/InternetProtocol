#pragma once

#include "ip/net/common.hpp"
#include "ip/utils/buffer.hpp"
#include "ip/utils/net.hpp"

namespace ip {
    class http_client_c {
    public:
        http_client_c() {}

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * http_client_c client;
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return net.socket.is_open(); }

        /**
         * Set address and port to resolve. Must be called before use 'request()' function.
         *
         * @param bind_opts An struct specifying protocol parameters to be used.
         * If 'port' is not specified or is 0, the operating system will attempt to bind to a random port.
         * If 'address' is not specified, the operating system will attempt to bind to local host
         *
         * @par Example
         * @code
         * http_client_c client;
         * client.set_host({});
         * @endcode
         */
        void set_host(const client_bind_options_t &bind_opts = {}) {
            bind_options = bind_opts;
        }

        /**
         * Process payload and send request.
         *
         * @param req Request data.
         *
         * @param response_cb lambada function to receive and process response.
         *
         * @par Example
         * @code
         * http_client_c client;
         * client.set_host({});
         *
         * net.request(req, [&](const asio::error_code &ec, const http_response_t &res) {
         *      if (ec) {
         *          // ec will be true if any error occur when trying to connect, send or receive payload
         *          std::cout << ec.message() << std::endl;
         *          return;
         *      }
         *      std::cout << res.status_code << " " << res.status_message << std::endl;
         *      std::cout << res.body << std::endl;
         * });
         * @endcode
         */
        void request(const http_request_t &req, const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (!net.socket.is_open()) {
                net.resolver.async_resolve(bind_options.protocol == v4 ? tcp::v4() : tcp::v6(),
                                            bind_options.address, bind_options.port,
                                            [&, req, response_cb](const asio::error_code &ec, const tcp::resolver::results_type &results) {
                                                resolve(ec, results, req, response_cb);
                                            });
                asio::post(thread_pool, [&]{ run_context_thread(); });
                return;
            }

            std::string payload = prepare_payload(req, net.socket.remote_endpoint().address().to_string(), net.socket.remote_endpoint().port());
            asio::async_write(net.socket,
                                asio::buffer(payload.data(), payload.size()),
                                [&, response_cb](const asio::error_code &ec, const size_t bytes_sent) {
                                    write_cb(ec, bytes_sent, response_cb);
                                });
        }

        /**
         * Close the underlying socket and stop listening for data on it. Also can be used to force cancel the request process.
         *
         * @param force Cancel all asynchronous operations associated with the socket if true.
         *
         * @par Example
         * @code
         * http_client_c client;
         * client.close();
         * @endcode
         */
        void close(const bool force = false) {
            is_closing.store(true);
            asio::error_code ec;
            if (force) {
                if (net.socket.is_open()) {
                    net.socket.cancel();
                    net.socket.close(ec);
                }
            } else {
                if (net.socket.is_open()) {
                    net.socket.shutdown(tcp::socket::shutdown_both, ec);
                    net.socket.close(ec);
                }
            }
            net.context.stop();
            net.context.restart();
            net.endpoint = tcp::endpoint();
            is_closing.store(false);
        }

    private:
        std::mutex mutex_io;
        std::atomic<bool> is_closing = false;
        client_bind_options_t bind_options;
        tcp_client_t net;
        asio::streambuf recv_buffer;

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            net.context.run();
            if (!is_closing.load())
                close();
        }

        void resolve(const asio::error_code &error, const tcp::resolver::results_type &results, const http_request_t &req, const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                response_cb(error, http_response_t());
                return;
            }

            net.endpoint = results.begin()->endpoint();
            asio::async_connect(net.socket,
                                results,
                                [&, req, response_cb](const asio::error_code &ec, const tcp::endpoint &ep) {
                                    conn(ec, req, response_cb);
                                });
        }

        void conn(const asio::error_code &error, const http_request_t &req, const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                response_cb(error, http_response_t());
                return;
            }

            consume_recv_buffer();
            std::string payload = prepare_payload(req, net.socket.remote_endpoint().address().to_string(), net.socket.remote_endpoint().port());
            asio::async_write(net.socket,
                                asio::buffer(payload.data(), payload.size()),
                                [&, response_cb](const asio::error_code &ec, const size_t bytes_sent) {
                                    write_cb(ec, bytes_sent, response_cb);
                                });
        }

        void write_cb(const asio::error_code &error, const size_t bytes_sent, const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                response_cb(error, http_response_t());
                return;
            }

            asio::async_read_until(net.socket,
                                recv_buffer, "\r\n",
                                [&, response_cb](const asio::error_code &ec, const size_t bytes_received) {
                                    read_cb(ec, bytes_received, response_cb);
                                });
        }

        void consume_recv_buffer() {
            const size_t size = recv_buffer.size();
            if (size > 0)
                recv_buffer.consume(size);
        }

        void read_cb(const asio::error_code &error, const size_t bytes_recvd, const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                response_cb(error, http_response_t());
                return;
            }

            http_response_t response;
            std::istream response_stream(&recv_buffer);
            std::string http_version;
            unsigned int status_code;
            std::string status_message;
            response_stream >> http_version;
            response_stream >> status_code;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                response.status_code = 505;
                response.status_message = "HTTP Version Not Supported";
                response_cb(error, response);
                return;
            }
            response.status_code = status_code;
            response.status_message = status_message;
            if (status_code != 200 && recv_buffer.size() == 0) {
                response_cb(error, response);
                return;
            }

            asio::async_read_until(net.socket,
                                    recv_buffer, "\r\n\r\n",
                                    std::bind(&http_client_c::read_headers,
                                                    this, asio::placeholders::error,
                                                    response,
                                                    response_cb));
        }

        void read_headers(const asio::error_code &error, http_response_t &response, const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                response_cb(error, response);
                return;
            }
            std::istream response_stream(&recv_buffer);
            std::string header;

            while (std::getline(response_stream, header) && header != "\r")
                res_append_header(response, header);

            if (recv_buffer.size() > 0) {
                std::ostringstream body_buffer;
                body_buffer << &recv_buffer;
                response.body = body_buffer.str();
            }

            consume_recv_buffer();
            response_cb(error, response);
        }
    };
}
