#define ASIO_USE_OPENSSL
#include <IP/InternetProtocol.hpp>
#include <iostream>

using namespace InternetProtocol;

int main() {
    TCPServer server;
    asio::error_code error_code;
    server.set_socket(Server::EServerProtocol::V4, 3000, 0, error_code);
    std::vector<std::shared_ptr<asio::ip::tcp::socket>> client;
    if (error_code) {
        std::cout << error_code.message() << std::endl;
        return -1;
    }
    server.on_accept = [&](const std::shared_ptr<asio::ip::tcp::socket> &peer) {
        std::cout << peer->remote_endpoint().port() << " -> " << "connected" << std::endl;
        client.push_back(peer);
    };
    server.on_disconnected = [&](const std::shared_ptr<asio::ip::tcp::socket> &peer) {
        std::cout << peer->remote_endpoint().port() << " -> " << "disconnected" << std::endl;
        client.erase(std::remove(client.begin(), client.end(), peer), client.end());
    };
    server.on_error = [&](const std::error_code& ec) {
        std::cout << "Error " << ec.value() << ": " << ec.message() << std::endl;
    };
    server.on_message_received = [&](const FTcpMessage message, const std::shared_ptr<asio::ip::tcp::socket> &peer) {
        std::cout << peer->remote_endpoint().port() << " -> " << message.toString() << std::endl;
    };
    if (!server.open()) return -1;

    std::string input;
    std::cout << "Digite 'quit' para sair!" << std::endl;
    while (std::getline(std::cin, input)) {
        if (input == "quit")
        {
            server.close();
            break;
        }
        if (input == "disconnect") {
            server.disconnect_peer(client[0]);
        }
        for (const std::shared_ptr<asio::ip::tcp::socket> &peer : server.get_peers()) {
            if (peer->is_open()) {
                server.send_str_to(input, peer);
            }
        }

    }
    return 0;
}
