#include <iostream>
#include <string>
#include <UDP/UDPClient.h>

using namespace InternetProtocol;

int main(int argc, char* argv[])
{
    UDPClient client;
    client.setThreadNumber(4);
    client.setHost("127.0.0.1", "3000");
    client.async_connect();
    client.onConnectionError = [&](){
        std::cout << "Error" << std::endl;
    };

    std::string str;
    std::cout << "Type 'quit' to exit." << "\n";
    std::cout << "Command: ";
    while (std::getline(std::cin, str)) {
        if (str == "quit") {

            break;
        }
    }

    return 0;
}