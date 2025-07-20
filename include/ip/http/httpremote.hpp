#pragma once

#include "ip/net/common.hpp"
#include "ip/utils/net.hpp"

using namespace asio::ip;

namespace internetprotocol {
    class http_remote_c {
    public:
        http_remote_c(asio::io_context &io_context): socket(io_context) {
        }

        bool is_open() const { return socket.is_open(); }

        tcp::endpoint local_endpoint() const { return socket.local_endpoint(); }

        tcp::endpoint remote_endpoint() const { return socket.remote_endpoint(); }

        tcp::socket &get_socket() { return socket; }

        http_response_t &get_response() { return response; }

        bool write(const std::function<void(const asio::error_code &, const size_t bytes_sent)> &callback = nullptr) {
            if (!socket.is_open())
                return false;

            std::string payload = prepare_response(response);
            std::cout << payload << std::endl;
            asio::async_write(socket,
                              asio::buffer(payload.data(), payload.size()),
                              [&, callback](const asio::error_code &ec, const size_t bytes_sent) {
                                  write_cb(ec, bytes_sent, callback);
                              });
            return true;
        }

        void connect() {
            asio::async_read_until(socket, recv_buffer, "\r\n",
                                   [&](const asio::error_code &ec, const size_t bytes_received) {
                                       read_cb(ec, bytes_received);
                                   });
        }

        void close(const bool force = false) {
            is_closing.store(true);
            if (force) {
                if (socket.is_open()) {
                    socket.cancel(); {
                        std::lock_guard guard(mutex_error);
                        socket.close(error_code);
                        if (error_code && on_error)
                            on_error(error_code);
                    }
                }
            } else {
                if (socket.is_open()) {
                    std::lock_guard guard(mutex_error);
                    socket.shutdown(tcp::socket::shutdown_both, error_code);
                    if (error_code && on_error)
                        on_error(error_code);
                    socket.close(error_code);
                    if (error_code && on_error)
                        on_error(error_code);
                }
            }
            is_closing.store(false);
        }

        std::function<void(const http_request_t &)> on_request;
        std::function<void()> on_close;
        std::function<void(const asio::error_code &)> on_error;

    private:
        std::mutex mutex_error;
        std::atomic<bool> is_closing = false;
        tcp::socket socket;
        //asio::steady_timer timer;
        asio::error_code error_code;
        bool will_close = false;
        http_response_t response;
        asio::streambuf recv_buffer;

        void consume_recv_buffer() {
            const size_t size = recv_buffer.size();
            if (size > 0)
                recv_buffer.consume(size);
        }

        void write_cb(const asio::error_code &error, const size_t bytes_sent, const std::function<void(const asio::error_code &ec, const size_t bytes_sent)> &callback) {
            if (callback) callback(error, bytes_sent);
            if (will_close)
                close();
        }

        void read_cb(const asio::error_code &error, const size_t bytes_recvd) {
            if (error) {
                consume_recv_buffer();
                if (on_error) on_error(error);
                if (on_close) on_close();
                return;
            }

            std::istream request_stream(&recv_buffer);
            std::string method;
            std::string path;
            std::string version;

            request_stream >> method;
            request_stream >> path;
            request_stream >> version;

            if (version != "HTTP/1.0" && version != "HTTP/1.1" && version != "HTTP/2.0") {
                response.status_code = 400;
                response.status_message = "Bad Request";
                response.body = "HTTP version not supported.";
                response.headers.insert_or_assign(Content_Type, "text/plain");
                response.headers.insert_or_assign(Content_Length, std::to_string(response.body.size()));
                will_close = true;
                write();
                return;
            }

            if (string_to_request_method(method) == UNKNOWN) {
                response.body = "Method not supported.";
                response.headers.insert_or_assign(Allow, "DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT, TRACE");
                response.headers.insert_or_assign(Content_Type, "text/plain");
                response.headers.insert_or_assign(Content_Length, std::to_string(response.body.size()));
                response.status_code = 400;
                response.status_message = "Bad Request";
                will_close = true;
                write();
                return;
            }

            version.erase(0, 5);
            asio::async_read_until(socket,
                                   recv_buffer, "\r\n\r\n",
                                   [&, path, version](const asio::error_code &ec, const size_t bytes_received) {
                                       read_headers(ec, path, version);
                                   });
        }

        void read_headers(const asio::error_code &error, const std::string &path, const std::string &version) {
            if (error) {
                consume_recv_buffer();
                if (on_error) on_error(error);
                if (on_close) on_close();
                return;
            }
            http_request_t req;
            req.version = version;
            req.path = path;

            std::istream request_stream(&recv_buffer);
            std::string header;

            while (std::getline(request_stream, header) && header != "\r")
                req_append_header(req, header);

            if (recv_buffer.size() > 0) {
                std::ostringstream body_buffer;
                body_buffer << &recv_buffer;
                req.body = body_buffer.str();
            }

            response.status_code = 200;
            response.status_message = "OK";
            response.headers.insert_or_assign(Content_Type, "text/plain");
            response.headers.insert_or_assign(X_Powered_By, "ASIO");

            consume_recv_buffer();
            if (on_request) on_request(req);
        }
    };
}
