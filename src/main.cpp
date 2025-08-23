/** 
 * MIT License (MIT)
 * Copyright © 2025 Nathan Miguel
*/

#define ENABLE_SSL
#include "ip.hpp"
#include <iostream>

using namespace internetprotocol;

int main(int argc, char** argv) {
    //ws_client_c net = ws_client_c();
    std::shared_ptr<ws_client_c> net = std::make_shared<ws_client_c>();

    net->on_error = [&](const asio::error_code &ec) {
        std::cout << ec.message() << std::endl;
    };
    net->on_message_received = [&](const std::vector<uint8_t> &buffer, bool is_binary) {
        std::cout << buffer_to_string(buffer) << std::endl;
    };
    net->on_connected = [&](const http_response_t &server_handshake) {
        std::cout << "Connected" << std::endl;
    };
    net->connect({"localhost", "8080", v4});

    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "quit") {
            net->close(1000, "Shutdown client");
            break;
        }
        net->write(input, {}, [](const asio::error_code &ec, size_t bytes_sent) {
            // Check if message has been sent
        });
    }
    join_threads();

    return 0;
}