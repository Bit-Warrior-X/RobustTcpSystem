// ServerApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#if 0
#include <iostream>
#include <string>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

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
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Bind the socket to a local address and port
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(m_szServerPort);  // Replace with your desired port number

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port 8000..." << std::endl;

    // Accept a client connection
    sockaddr_in clientAddr;
    int clientAddrSize = sizeof(clientAddr);
    SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Client connected!" << std::endl;

    // Receive data from the client
    char buffer[1024];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived > 0) {
        std::cout << "Received data: " << std::string(buffer, 0, bytesReceived) << std::endl;
    }
    else {
        std::cerr << "Receive failed: " << WSAGetLastError() << std::endl;
    }

    // Send data to the client
    std::string message = "Hello from the server!";
    int bytesSent = send(clientSocket, message.c_str(), message.length(), 0);
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
    }

    // Clean up
    closesocket(clientSocket);
    closesocket(serverSocket);
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

// Coroutine for handling client sessions
awaitable<void> session(ip::tcp::socket socket) {
    try {
        char data[1024];
        for (;;) {
            std::size_t n = co_await socket.async_read_some(buffer(data), use_awaitable);
            if (n == 0) {
                break; // Connection closed by client
            }
            std::string message(data, n);
            std::cout << "Received: " << message << std::endl;

            std::string response = "Server response: " + message;
            co_await async_write(socket, buffer(response), use_awaitable);
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    std::cout << "Client disconnected" << std::endl;
}

// Coroutine for listening for incoming connections
awaitable<void> listener() {
    auto executor = co_await this_coro::executor;
    ip::tcp::acceptor acceptor(executor, { ip::tcp::v4(), 8000 });

    std::cout << "Server listening on port 8000" << std::endl;

    for (;;) {
        ip::tcp::socket socket = co_await acceptor.async_accept(use_awaitable);
        co_spawn(executor, session(std::move(socket)), detached);
    }
}

int main() {
    try {
        io_context ctx;
        co_spawn(ctx, listener(), detached);
        ctx.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
