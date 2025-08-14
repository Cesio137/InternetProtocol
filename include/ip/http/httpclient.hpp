#pragma once

#include "ip/net/common.hpp"
#include "ip/utils/net.hpp"

namespace internetprotocol {
    class http_client_c {
    public:
        http_client_c(): idle_timer(net.context) {
        }

        ~http_client_c() {
            if (net.socket.is_open())
                close();
            consume_recv_buffer();
        }

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
         * Set timeout value in seconds
         *
         * Timeout is always reseted when socket send or receive data
         *
         * @par Example
         * @code
         * http_client_c client;
         * client.set_timeout(5);
         * @endcode
         */
         void set_timeout(uint16_t value) { idle_timeout_seconds = value; }

        /**
         * Get timeout value in seconds
         *
         * @par Example
         * @code
         * http_client_c client;
         * client.set_timeout(5);
         * @endcode
         */
        uint16_t get_timeout() const { return idle_timeout_seconds; }

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
        void request(const http_request_t &req,
                     const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (!net.socket.is_open()) {
                net.resolver.async_resolve(bind_options.protocol == v4 ? tcp::v4() : tcp::v6(),
                                           bind_options.address, bind_options.port,
                                           [&, req, response_cb](const asio::error_code &ec,
                                                                 const tcp::resolver::results_type &results) {
                                               resolve(ec, results, req, response_cb);
                                           });
                asio::post(thread_pool, [&] { run_context_thread(); });
                return;
            }

            std::string payload = prepare_request(req, net.socket.remote_endpoint().address().to_string(),
                                                  net.socket.remote_endpoint().port());
            if (idle_timeout_seconds > 0)
                reset_idle_timer();
            asio::async_write(net.socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&, response_cb](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_cb(ec, bytes_sent, response_cb);
                              });
        }

        /**
         * Close the underlying socket and stop listening for data on it. Also can be used to force cancel the request process.
         *
         * @par Example
         * @code
         * http_client_c client;
         * client.close();
         * @endcode
         */
        void close() {
            is_closing.store(true);
            asio::error_code ec;
            if (net.socket.is_open()) {
                net.socket.shutdown(tcp::socket::shutdown_both, ec);
                net.socket.close(ec);
            }
            net.context.stop();
            net.context.restart();
            net.endpoint = tcp::endpoint();
            is_closing.store(false);
        }

    private:
        std::mutex mutex_io;
        std::atomic<bool> is_closing = false;
        asio::steady_timer idle_timer;
        uint16_t idle_timeout_seconds = 0;
        client_bind_options_t bind_options;
        tcp_client_t net;
        asio::streambuf recv_buffer;

        void start_idle_timer() {
            if (idle_timeout_seconds == 0)
                return;

            idle_timer.expires_after(std::chrono::seconds(idle_timeout_seconds));
            idle_timer.async_wait([&](const asio::error_code &ec) {
                if (ec == asio::error::operation_aborted)
                    return;

                if (is_closing.load())
                    return;

                close();
            });
        }

        void reset_idle_timer() {
            if (is_closing.load() || idle_timeout_seconds == 0)
                return;

            idle_timer.cancel();
            start_idle_timer();
        }

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            net.context.run();
            if (!is_closing.load())
                close();
        }

        void resolve(const asio::error_code &error, const tcp::resolver::results_type &results,
                     const http_request_t &req,
                     const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
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

        void conn(const asio::error_code &error, const http_request_t &req,
                  const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                response_cb(error, http_response_t());
                return;
            }

            std::string payload = prepare_request(req, net.socket.remote_endpoint().address().to_string(),
                                                  net.socket.remote_endpoint().port());
            if (idle_timeout_seconds > 0)
                start_idle_timer();
            asio::async_write(net.socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&, response_cb](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_cb(ec, bytes_sent, response_cb);
                              });
        }

        void write_cb(const asio::error_code &error, const size_t bytes_sent,
                      const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                response_cb(error, http_response_t());
                return;
            }


            consume_recv_buffer();
            if (idle_timeout_seconds > 0)
                reset_idle_timer();
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

        void read_cb(const asio::error_code &error, const size_t bytes_recvd,
                     const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                consume_recv_buffer();
                response_cb(error, http_response_t());
                return;
            }
            if (idle_timeout_seconds > 0)
                reset_idle_timer();

            http_response_t response;
            std::istream response_stream(&recv_buffer);
            std::string http_version;
            unsigned int status_code;
            std::string status_message;
            response_stream >> http_version;
            response_stream >> status_code;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                consume_recv_buffer();
                response.status_code = 505;
                response.status_message = "HTTP Version Not Supported";
                response_cb(error, response);
                return;
            }
            response.status_code = status_code;
            response.status_message = status_message;
            if (status_code != 200 && recv_buffer.size() == 0) {
                consume_recv_buffer();
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

        void read_headers(const asio::error_code &error, http_response_t &response,
                          const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                consume_recv_buffer();
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
#ifdef ENABLE_SSL
    class http_client_ssl_c {
    public:
        explicit http_client_ssl_c(const security_context_opts &sec_opts = {}): idle_timer(net.context) {
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

            net.ssl_socket = asio::ssl::stream<tcp::socket>(net.context, net.ssl_context);
        }

        ~http_client_ssl_c() {
        }

        /**
         * Return true if socket is open.
         *
         * @par Example
         * @code
         * http_client_ssl_c client({});
         * bool is_open = client.is_open();
         * @endcode
         */
        bool is_open() const { return net.ssl_socket.next_layer().is_open(); }

        /**
         * Set timeout value in seconds
         *
         * Timeout is always reseted when socket send or receive data
         *
         * @par Example
         * @code
         * http_client_c client;
         * client.set_timeout(5);
         * @endcode
         */
        void set_timeout(uint16_t value) { idle_timeout_seconds = value; }

        /**
         * Get timeout value in seconds
         *
         * @par Example
         * @code
         * http_client_c client;
         * client.set_timeout(5);
         * @endcode
         */
        uint16_t get_timeout() const { return idle_timeout_seconds; }

        /**
         * Set address and port to resolve. Must be called before use 'request()' function.
         *
         * @param bind_opts An struct specifying protocol parameters to be used.
         * If 'port' is not specified or is 0, the operating system will attempt to bind to a random port.
         * If 'address' is not specified, the operating system will attempt to bind to local host
         *
         * @par Example
         * @code
         * http_client_ssl_c client({});
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
         * http_client_ssl_c client({});
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
        void request(const http_request_t &req,
                     const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (!net.ssl_socket.next_layer().is_open()) {
                net.resolver.async_resolve(bind_options.protocol == v4 ? tcp::v4() : tcp::v6(),
                                           bind_options.address, bind_options.port,
                                           [&, req, response_cb](const asio::error_code &ec,
                                                                 const tcp::resolver::results_type &results) {
                                               resolve(ec, results, req, response_cb);
                                           });
                asio::post(thread_pool, [&] { run_context_thread(); });
                return;
            }

            std::string payload = prepare_request(
                req, net.ssl_socket.next_layer().remote_endpoint().address().to_string(),
                net.ssl_socket.next_layer().remote_endpoint().port());
            if (idle_timeout_seconds > 0)
                reset_idle_timer();
            asio::async_write(net.ssl_socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&, response_cb](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_cb(ec, bytes_sent, response_cb);
                              });
        }

        /**
         * Close the underlying socket and stop listening for data on it. Also can be used to force cancel the request process.
         *
         * @par Example
         * @code
         * http_client_ssl_c client({});
         * client.close();
         * @endcode
         */
        void close() {
            is_closing.store(true);
            asio::error_code ec;
            if (net.ssl_socket.next_layer().is_open()) {
                net.ssl_socket.lowest_layer().shutdown(asio::socket_base::shutdown_both, ec);
                net.ssl_socket.lowest_layer().close(ec);

                net.ssl_socket.shutdown(ec);
                net.ssl_socket.next_layer().close(ec);
            }
            net.context.stop();
            net.context.restart();
            net.endpoint = tcp::endpoint();
            is_closing.store(false);
        }

    private:
        std::mutex mutex_io;
        std::atomic<bool> is_closing = false;
        asio::steady_timer idle_timer;
        uint16_t idle_timeout_seconds = 0;
        client_bind_options_t bind_options;
        tcp_client_ssl_t net;
        asio::streambuf recv_buffer;

        void start_idle_timer() {
            if (idle_timeout_seconds == 0)
                return;

            idle_timer.expires_after(std::chrono::seconds(idle_timeout_seconds));
            idle_timer.async_wait([&](const asio::error_code &ec) {
                if (ec == asio::error::operation_aborted)
                    return;

                if (is_closing.load())
                    return;

                close();
            });
        }

        void reset_idle_timer() {
            if (is_closing.load() || idle_timeout_seconds == 0)
                return;

            idle_timer.cancel();
            start_idle_timer();
        }

        void run_context_thread() {
            std::lock_guard guard(mutex_io);
            net.context.run();
            if (!is_closing.load())
                close();
        }

        void resolve(const asio::error_code &error, const tcp::resolver::results_type &results,
                     const http_request_t &req,
                     const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                response_cb(error, http_response_t());
                return;
            }

            net.endpoint = results.begin()->endpoint();
            asio::async_connect(net.ssl_socket.lowest_layer(),
                                results,
                                [&, req, response_cb](const asio::error_code &ec, const tcp::endpoint &ep) {
                                    conn(ec, req, response_cb);
                                });
        }

        void conn(const asio::error_code &error,
                  const http_request_t &req,
                  const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                response_cb(error, http_response_t());
                return;
            }

            net.ssl_socket.async_handshake(asio::ssl::stream_base::client,
                                           [&, req, response_cb](const asio::error_code &ec) {
                                               ssl_handshake(ec, req, response_cb);
                                           });
        }

        void ssl_handshake(const asio::error_code &error,
                           const http_request_t &req,
                           const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                response_cb(error, http_response_t());
                return;
            }

            std::string payload = prepare_request(req, net.ssl_socket.next_layer().remote_endpoint().address().to_string(),
                                                  net.ssl_socket.next_layer().remote_endpoint().port());
            if (idle_timeout_seconds > 0)
                start_idle_timer();
            asio::async_write(net.ssl_socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&, response_cb](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_cb(ec, bytes_sent, response_cb);
                              });
        }

        void write_cb(const asio::error_code &error, const size_t bytes_sent,
                      const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                response_cb(error, http_response_t());
                return;
            }

            consume_recv_buffer();
            if (idle_timeout_seconds > 0)
                reset_idle_timer();
            asio::async_read_until(net.ssl_socket,
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

        void read_cb(const asio::error_code &error, const size_t bytes_recvd,
                     const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                consume_recv_buffer();
                response_cb(error, http_response_t());
                return;
            }
            if (idle_timeout_seconds > 0)
                reset_idle_timer();

            http_response_t response;
            std::istream response_stream(&recv_buffer);
            std::string http_version;
            unsigned int status_code;
            std::string status_message;
            response_stream >> http_version;
            response_stream >> status_code;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                consume_recv_buffer();
                response.status_code = 505;
                response.status_message = "HTTP Version Not Supported";
                response_cb(error, response);
                return;
            }
            response.status_code = status_code;
            response.status_message = status_message;
            if (status_code != 200 && recv_buffer.size() == 0) {
                consume_recv_buffer();
                response_cb(error, response);
                return;
            }

            asio::async_read_until(net.ssl_socket,
                                   recv_buffer, "\r\n\r\n",
                                   std::bind(&http_client_ssl_c::read_headers,
                                             this, asio::placeholders::error,
                                             response,
                                             response_cb));
        }

        void read_headers(const asio::error_code &error, http_response_t &response,
                          const std::function<void(const asio::error_code &, const http_response_t &)> &response_cb) {
            if (error) {
                consume_recv_buffer();
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
#endif
}
