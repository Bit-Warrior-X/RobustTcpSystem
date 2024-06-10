// ClientApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#if 0
#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

const char* m_szServerIP = "127.0.0.1";
int m_szServerPort = 8000;

int main() {
    // Initialize Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    // Create a socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Connect to the server
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;

    std::string serverIP = m_szServerIP;
    InetPton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

    serverAddr.sin_port = htons(m_szServerPort);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connect failed: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to the server!" << std::endl;

    // Send data to the server
    std::string message = "Hello from the client!";
    int bytesSent = send(clientSocket, message.c_str(), (int)message.length(), 0);
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
    }

    // Receive data from the server
    char buffer[1024];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived > 0) {
        std::cout << "Received data: " << std::string(buffer, 0, bytesReceived) << std::endl;
    }
    else {
        std::cerr << "Receive failed: " << WSAGetLastError() << std::endl;
    }

    // Clean up
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
#endif

#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <nlohmann/json.hpp>

using namespace boost::asio;

awaitable<void> client() {
    auto executor = co_await this_coro::executor;
    ip::tcp::socket socket(executor);

    try {
        co_await socket.async_connect({ ip::address::from_string("127.0.0.1"), 8000 }, use_awaitable);
        std::cout << "Connected to server" << std::endl;

        // Create a JSON object
        nlohmann::json jsonObj;

        // Add key-value pairs to the JSON object
        jsonObj["action"] = "signin";
        jsonObj["userid"] = "";

        // Convert JSON object to string (pretty print with indentation of 4 spaces)
        std::string jsonString = jsonObj.dump();

        char data[1024];
        for (;;) {
            std::cout << "Enter message: ";
            std::string message;
            std::getline(std::cin, message);

            if (message == "exit") {
                break;
            }

            co_await async_write(socket, buffer(message), use_awaitable);

            std::size_t n = co_await socket.async_read_some(buffer(data), use_awaitable);
            std::string response(data, n);
            std::cout << "Server response: " << response << std::endl;
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    try {
        io_context ctx;
        co_spawn(ctx, client(), detached);
        ctx.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
