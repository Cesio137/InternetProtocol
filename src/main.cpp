#define ENABLE_SSL
#include <fstream>
#include <iostream>
#include "ip.hpp"

using namespace internetprotocol;

std::string loadfile(const std::string& file) {
    std::ifstream arquivo(file);

    if (!arquivo.is_open()) {
        throw std::runtime_error("Erro trying to open file: " + file);
    }

    std::stringstream buffer;
    buffer << arquivo.rdbuf();

    return buffer.str();
}

const std::string cert = loadfile("cert.pem");
const std::string key = loadfile("key.pem");
const std::string csr = loadfile("csr.pem");
const std::string ca_cert = loadfile("ca-cert.pem");

int main(int argc, char** argv) {
    http_server_ssl_c net({key, cert, "", "", file_format_e::pem, none, ""});

    net.on_error = [&](const asio::error_code &ec) {
        std::cout << ec.message() << std::endl;
    };
    net.get("/", [&](const http_request_t &request,  const std::shared_ptr<http_remote_ssl_c> &response) {
        response->on_close = []() {
            std::cout << "close" << std::endl;
        };
        response->on_error = [&](const asio::error_code &ec) {
            std::cout << ec.message() << std::endl;
        };
        std::cout << request.headers.at(Connection) << std::endl;
        http_response_t &headers = response->headers();
        headers.body = "Your remote port is: " + std::to_string(response->remote_endpoint().port());
        response->write();
    });
    net.open({"", 8080});

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
    http_client_c http_client;
    tcp_server_ssl_c net({ key, cert, "", "", file_format_e::pem, none, "" });
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
    net.open({"", 8080});

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