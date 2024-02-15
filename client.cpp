#include <iostream>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <fstream>
#include <filesystem>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

const int PORT = 12345;
const CHAR *SERVER_IP = "127.0.0.1";

class Client {
public:
    Client() : clientSocket(0) {
        // Initialize Winsock
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            exit(EXIT_FAILURE);
        }

        // Create client socket
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
            WSACleanup();
            exit(EXIT_FAILURE);
        }

        // Configure server address
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(PORT);
        InetPton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

        // Connect to the server
        if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Connect failed with error: " << WSAGetLastError() << std::endl;
            closesocket(clientSocket);
            WSACleanup();
            exit(EXIT_FAILURE);
        }

        enterName();
        enterRoom();
        std::cout << "Connected to server and joined room " << roomID << std::endl;

        // Create a new thread to handle receiving messages
        std::thread([this]() {
            receiveMessages();
        }).detach();
        sendMessages();
    }

    ~Client() {
        closesocket(clientSocket);
        WSACleanup();
    }

private:
    SOCKET clientSocket;
    sockaddr_in serverAddr{};
    WSADATA wsaData;
    std::string roomID;
    std::string userName;

    void enterRoom() {
        std::cout << "Enter room ID: ";
        std::cin >> roomID;
        send(clientSocket, roomID.c_str(), roomID.size(), 0);
    }

    void enterName() {
        std::cout << "Enter your name: ";
        std::getline(std::cin, userName);
        send(clientSocket, userName.c_str(), userName.size(), 0);
    }

    void receiveMessages() {
        char buffer[1024];
        while (true) {
            memset(buffer, 0, sizeof(buffer));
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0) {
                std::cerr << "Server disconnected" << std::endl;
                break;
            }
            std::cout  << buffer << std::endl;
        }
    }

    void sendMessages() {
        char buffer[1024];
        while (true) {
            std::cin.getline(buffer, sizeof(buffer));
            send(clientSocket, buffer, strlen(buffer), 0);
        }
    }
};

int main() {
    Client client;

    std::this_thread::sleep_for(std::chrono::hours(1));

    return 0;
}