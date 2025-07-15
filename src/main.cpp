#define ENABLE_SSL
#include <iostream>
#include "ip.hpp"

using namespace ip;

int main(int argc, char** argv) {
    std::cout << "hello world!" << std::endl;
    tcp_server_c net;
    /*
    net.set_socket(v4, 8080);
    std::set<std::shared_ptr<tcp::socket>> sockets;
    net.on_socket_accepted = [&](const std::shared_ptr<tcp::socket> &sock) {
        sockets.insert(sock);
        std::cout << sock->local_endpoint().address() << ":" << sock->local_endpoint().port() << " -> " << "login" << std::endl;
    };
    net.on_socket_disconnected = [&](const asio::error_code &ec, const std::shared_ptr<tcp::socket> &sock) {
        if (!sock) return;
        if (sockets.find(sock) != sockets.end())
            sockets.erase(sock);
        //std::cout << sock->local_endpoint().address() << ":" << sock->local_endpoint().port() << " -> " << "logout" << std::endl;
    };
    net.on_message_received = [&](const std::vector<uint8_t> &msg, const std::shared_ptr<tcp::socket> &sock) {
        std::cout << buffer_to_string(msg) << std::endl;
    };
    net.open();

    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "quit") {
            net.close();
            break;
        }
        for (const auto &socket: sockets) {
            net.write_to(input, socket);
            net.write_to("teste", socket);
        }
    }
    join_threads();*/

    return 0;
}
