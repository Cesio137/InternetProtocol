#include <iostream>
#include <string>
#include <UDP/UDPClient.h>

using namespace InternetProtocol;

int main(int argc, char* argv[])
{
    UDPClient client;
    client.setThreadNumber(4);
    client.setHost("localhost", "3000");
    client.onConnected = [&](){
        std::cout << client.isOpen() << std::endl;
    };
    client.onMessageReceived = [&](int BytesReceived, const std::string& message) {
        std::cout << "Message size: " << BytesReceived << std::endl;
        std::cout << "Message: " << message << std::endl;
    };
    client.onMessageReceivedError = [&](int code, const std::string& message) {
        std::cout << "Message error: " << message << std::endl;
    };
    client.onConnectionError = [&](int code, const std::string& message) {
        std::cout << "Error: " << message << std::endl;
    };
    client.connect();    

    std::string str;
    std::cout << "Type 'quit' to exit." << std::endl;
    while (std::getline(std::cin, str)) {
        if (str == "quit")
            break;
        client.send(str);
    }

    return 0;
}