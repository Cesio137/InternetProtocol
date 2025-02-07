#include <iostream>
#define ASIO_USE_OPENSSL
#include "IP/InternetProtocol.hpp"

std::map<std::string, std::string> headers;
std::map<std::string, std::string> &get_headers() {
    return headers;
}

int main() {
    std::map<std::string, std::string> headers2;
    headers2.insert_or_assign("Olá", "Mundo");
    get_headers() = headers2;
    //get_headers().insert_or_assign("Olá", "Mundo");
    std::cout << get_headers().at("Olá") << std::endl;
    std::cout << headers.at("Olá") << std::endl;

    return 0;
}
