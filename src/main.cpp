#define ENABLE_SSL
#include <iostream>
#include "ip.hpp"

using namespace internetprotocol;

http_request_t req = {
    GET,
    "/",
    "1.1",
    {},
    {
                {Accept, "/*"},
                {User_Agent, "ASIO"},
                {Connection, "close"}
    },
    "",
};

int main(int argc, char** argv) {
    http_server_c net;

    net.on_error = [&](const asio::error_code &ec) {
        std::cout << ec.message() << std::endl;
    };
    net.get("/", [&](const http_request_t &request,  const std::shared_ptr<http_remote_c> &response) {
        http_response_t &res = response->get_response();
        res.body = "Your remote port is: " + std::to_string(response->remote_endpoint().port());
        response->write();
    });
    net.open({});

    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "quit") {
            net.close();
            break;
        }
    }
    join_threads();

    return 0;
}

/*
int main(int argc, char** argv) {
    std::cout << "hello world!" << std::endl;
    http_client_c http_client;
    tcp_server_ssl_c net({ "", "", "", "", file_format_e::pem, verify_peer, "" });
    net.on_client_accepted = [&](const std::shared_ptr<tcp_remote_ssl_c> &remote) {
        std::cout << remote->local_endpoint().address() << ":" << remote->local_endpoint().port() << " -> " << "login" << std::endl;
        const int port = remote->local_endpoint().port();
        remote->on_message_received = [&, port](const std::vector<uint8_t> &buffer, const size_t bytes_recvd) {
            std::cout << port << " -> " << buffer_to_string(buffer) << std::endl;
        };
        remote->on_close = [&, port]() {
            std::cout << port << " -> " << "logout" << std::endl;
        };
        remote->on_error = [&](const asio::error_code &ec) {
            std::cout << ec.message() << std::endl;
        };
    };
    net.open();

    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "quit") {
            net.close();
            break;
        }
        for (const auto &client: net.clients()) {
            client->write(input);
        }
    }
    join_threads();

    return 0;
}
*/