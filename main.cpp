#include <iostream>
#include <string>
#include <Websocket/WebsocketClient.h>
#include <TCP/TCPClient.h>
#include <UDP/UDPClient.h>

using namespace InternetProtocol;

int main(int argc, char* argv[])
{
    WebsocketClient client;
    client.setHost("localhost", "3000");
    client.onConnected = []() {
        std::cout << "Connected." << std::endl;
    };
    client.onError = [](int code, const std::string& message) {
        std::cout << "Error code: " << code << std::endl;
        std::cout << "Error message: " << message << std::endl;
    };
    client.onMessageReceived = [](int size, const FDataFrame message) {
        std::cout << "Message size: " << size << std::endl;
        std::cout << "Message: " << message.toUTF8() << std::endl;
    };
    client.connect();

    std::string str;
    std::cout << "Type 'quit' to exit." << std::endl;
    while (std::getline(std::cin, str)) {
        if (str == "quit") {
            client.close();
            break;
        }
        client.send(str);
    }

    return 0;
}