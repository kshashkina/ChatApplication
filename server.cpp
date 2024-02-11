#include <iostream>
#include <vector>
#include <WinSock2.h>
#include <thread>
#include <mutex>
#pragma comment(lib, "ws2_32.lib")

class Server {
public:
    Server(int port) : serverSocket(0), port(port) {
        // Initialize Winsock
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            exit(EXIT_FAILURE);
        }

        // Create server socket
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == INVALID_SOCKET) {
            std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
            WSACleanup();
            exit(EXIT_FAILURE);
        }

        // Configure server address
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        // Bind the socket
        if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
            closesocket(serverSocket);
            WSACleanup();
            exit(EXIT_FAILURE);
        }

        // Listen for incoming connections
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
            closesocket(serverSocket);
            WSACleanup();
            exit(EXIT_FAILURE);
        }

        std::cout << "Server listening on port " << port << std::endl;
    }

    void start() {
        while (true) {
            SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
                closesocket(serverSocket);
                WSACleanup();
                exit(EXIT_FAILURE);
            }

            std::cout << "New client connected" << std::endl;
            clients.push_back(clientSocket);

            // Create a new thread for each client to handle communication
            std::lock_guard<std::mutex> lock(m);
            std::thread([this, clientSocket]() {
                handleClient(clientSocket);
            }).detach();
        }
    }

private:
    int port;
    SOCKET serverSocket;
    sockaddr_in serverAddr{};
    WSADATA wsaData;
    std::vector<SOCKET> clients;
    std::mutex m;

    void handleClient(SOCKET clientSocket) {
        char buffer[1024];
        while (true) {
            memset(buffer, 0, sizeof(buffer));
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);

            if (bytesRead <= 0) {
                std::cout << "Client disconnected" << std::endl;
                for (auto it = clients.begin(); it != clients.end(); ++it) {
                    if (*it == clientSocket) {
                        clients.erase(it);
                        break;
                    }
                }

                closesocket(clientSocket);
                break;
            }

            for (SOCKET client : clients) {
                if (client != clientSocket) {
                    send(client, buffer, bytesRead, 0);
                }
            }
        }
    }
};

int main() {
    const int port = 12345;
    Server server(port);
    server.start();

    return 0;
}