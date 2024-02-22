#include <iostream>
#include <vector>
#include <map>
#include <WinSock2.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include <string>
#include <fstream>
#include <filesystem>
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

            std::string message(buffer, bytesRead);

            if (message.find("CHANGE ") == 0) {
                std::string newRoomID = message.substr(7);
                removeClientFromRoom(clientSocket, roomID);
                roomID = newRoomID;
                addClientToRoom(clientSocket, roomID, clientName);

                std::string successMessage = "You have successfully switched to room " + newRoomID;
                send(clientSocket, successMessage.c_str(), successMessage.length(), 0);
            } else if (message.find("EXIT") == 0) {
                removeClientFromRoom(clientSocket, roomID);
                closesocket(clientSocket);
            }
            else if (message.find("SEND") == 0){
                getFile(clientSocket);
            }
            else if (message.find("ACCEPT") == 0){
                std::string type = "FILE";
                send(clientSocket, type.c_str(), type.length(),0);
                for (const auto& entry : std::filesystem::directory_iterator("C:\\KSE IT\\Client Server Concepts\\csc_third\\serverStorage")) {
                    if (entry.is_regular_file()) {
                        std::string fileName = entry.path().filename().string();
                        std::uintmax_t fileSize = std::filesystem::file_size(entry.path());
                        sendFile(clientSocket, fileName, fileSize);
                    }
                }

            }
            else {
                sendMessageToRoom(roomID, clientName, buffer, bytesRead);
            }
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

    void sendMessageToRoom(const std::string &roomID, const std::string &clientName, const char *message, int messageSize) {
        std::lock_guard<std::mutex> lock(roomMutex);
        std::string fullMessage = clientName + ": " + std::string(message, messageSize);
        for (const auto &client: rooms[roomID]) {
            if (client.name != clientName) { // Don't send the message to the sender
                send(client.socket, fullMessage.c_str(), fullMessage.length(), 0);
            }
        }
    }

    void sendFile(SOCKET clientSocket, const std::string& fileName,  std::uintmax_t fileSize){
        std::ifstream fileToSend("C:\\KSE IT\\Client Server Concepts\\csc_third\\serverStorage\\" + fileName , std::ios::binary);
        if (!fileToSend.is_open()) {
            std::cerr << "Failed to open file for reading: " << fileName << std::endl;
            return;
        }

        const int chunkSize = 1024;
        char fileBuffer[chunkSize];

        send(clientSocket, fileName.c_str(), fileName.length(), 0);
        send(clientSocket, reinterpret_cast<const char*>(&fileSize), sizeof(fileSize), 0);

        while (!fileToSend.eof()) {
            fileToSend.read(fileBuffer, chunkSize);
            int bytesRead = static_cast<int>(fileToSend.gcount());
            send(clientSocket, fileBuffer, bytesRead, 0);
        }

        std::cout << "File sent to client " << getClientName(clientSocket) << std::endl;


        fileToSend.close();

    }

    void getFile(SOCKET clientSocket) {
        char buffer[1024];
        int bytesRead;

        memset(buffer, 0, sizeof(buffer));
        bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            std::cerr << "Failed to get file name or client disconnected" << std::endl;
            closesocket(clientSocket);
            return;
        }
        std::string fileName(buffer, bytesRead);

        int fileSize;
        bytesRead = recv(clientSocket, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0);
        if (bytesRead != sizeof(fileSize)) {
            std::cerr << "Failed to get file size or client disconnected" << std::endl;
            closesocket(clientSocket);
            return;
        }

        std::ofstream outputFile("C:\\KSE IT\\Client Server Concepts\\csc_third\\serverStorage\\" + fileName, std::ios::binary);
        if (!outputFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << fileName << std::endl;
            return;
        }

        int totalBytesReceived = 0;
        while (totalBytesReceived < fileSize) {
            bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            outputFile.write(buffer, bytesRead);
            totalBytesReceived += bytesRead;
        }

        outputFile.close();
        std::cout << "File received: " << fileName << std::endl;

        std::string notification = "User " + getClientName(clientSocket) + " wants to send you a file named '" + fileName + "' (" + std::to_string(fileSize) + " bytes). Do you want to accept? (ACCEPT/NO)";
        sendRequest(getClientRoomID(clientSocket), clientSocket, notification);
    }

    std::string getClientName(SOCKET clientSocket) {
        std::string clientName;
        for (const auto& [roomID, clients] : rooms) {
            for (const auto& client : clients) {
                if (client.socket == clientSocket) {
                    clientName = client.name;
                    break;
                }
            }
        }
        return clientName;
    }

    std::string getClientRoomID(SOCKET clientSocket) {
        std::string roomID;
        for (const auto& [rID, clients] : rooms) {
            for (const auto& client : clients) {
                if (client.socket == clientSocket) {
                    roomID = rID;
                    break;
                }
            }
        }
        return roomID;
    }

    void sendRequest(const std::string& roomID, SOCKET senderSocket, const std::string& message) {
        for (const auto& client : rooms[roomID]) {
            if (client.socket != senderSocket) {
                send(client.socket, message.c_str(), message.length(), 0);
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