#include <iostream>
#include <vector>
#include <map>
#include <WinSock2.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include <string>
#pragma comment(lib, "ws2_32.lib")

struct ClientInfo {
    SOCKET socket;
    std::string name;
};

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
                continue;
            }

            std::cout << "New client connected" << std::endl;
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
    std::map<std::string, std::vector<ClientInfo>> rooms;
    std::mutex roomMutex;

    void handleClient(SOCKET clientSocket) {
        char buffer[1024];
        // Receive client name
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            std::cerr << "Failed to get client name or client disconnected" << std::endl;
            closesocket(clientSocket);
            return;
        }
        std::string clientName(buffer, bytesRead);

        // Receive room ID
        memset(buffer, 0, sizeof(buffer));
        bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            std::cerr << "Failed to get room ID or client disconnected" << std::endl;
            closesocket(clientSocket);
            return;
        }
        std::string roomID(buffer, bytesRead);
        addClientToRoom(clientSocket, roomID, clientName);

        while (true) {
            memset(buffer, 0, sizeof(buffer));
            bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0) {
                std::cerr << "Client disconnected" << std::endl;
                removeClientFromRoom(clientSocket, roomID);
                closesocket(clientSocket);
                break;
            }

            sendMessageToRoom(roomID, clientName, buffer, bytesRead);
        }
    }

    void addClientToRoom(SOCKET clientSocket, const std::string &roomID, const std::string &clientName) {
        std::lock_guard<std::mutex> lock(roomMutex);
        rooms[roomID].push_back({clientSocket, clientName});
        std::cout << "Client " << clientName << " added to room " << roomID << std::endl;

        std::string message = clientName + " has joined the room.";
        for (const auto& client : rooms[roomID]) {
            if (client.socket != clientSocket) {
                send(client.socket, message.c_str(), message.length(), 0);
            }
        }
    }

    void removeClientFromRoom(SOCKET clientSocket, const std::string& roomID) {
        std::lock_guard<std::mutex> lock(roomMutex);
        std::string clientName;
        auto& clients = rooms[roomID];
        auto it = std::find_if(clients.begin(), clients.end(), [clientSocket, &clientName](const ClientInfo& ci) {
            if (ci.socket == clientSocket) {
                clientName = ci.name;
                return true;
            }
            return false;
        });

        if (it != clients.end()) {
            clients.erase(it);
            std::cout << "Client " << clientName << " removed from room " << roomID << std::endl;

            std::string message = clientName + " has left the room.";
            for (const auto& client : clients) {
                send(client.socket, message.c_str(), message.length(), 0);
            }
        }
    }


    void
    sendMessageToRoom(const std::string &roomID, const std::string &clientName, const char *message, int messageSize) {
        std::lock_guard<std::mutex> lock(roomMutex);
        std::string fullMessage = clientName + ": " + std::string(message, messageSize);
        for (const auto &client: rooms[roomID]) {
            if (client.name != clientName) { // Don't send the message to the sender
                send(client.socket, fullMessage.c_str(), fullMessage.length(), 0);
            }
        }
    }

    void WSACleanup() {
        WSACleanup();
    }
};

int main() {
    const int port = 12345;
    Server server(port);
    server.start();
    return 0;
};