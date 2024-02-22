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
            if (strcmp(buffer, "FILE") == 0) {
                receiveFile();
            }else{
            std::cout << buffer << std::endl;
            }
        }
    }

    void sendMessages() {
        std::string message;
        while (true) {
            std::getline(std::cin, message);
            if (message == "EXIT") {
                send(clientSocket, message.c_str(), message.length(), 0);

                std::cout << "Exiting the room and disconnecting..." << std::endl;
                closesocket(clientSocket);
                WSACleanup();
                exit(0);
            } else if (message.substr(0, 4) == "SEND"){
                send(clientSocket, "SEND", 4, 0);
                std::string filePath = message.substr(5);
                sendFile(filePath);
                continue;
            }
            else {
                send(clientSocket, message.c_str(), message.length(), 0);
            }
        }
    };

    void receiveFile() {
        ensureUserFolderExists();
        const int chunkSize = 1024;
        char fileBuffer[chunkSize];

        // Receive the file name
        char fileNameBuffer[256]; // Assuming maximum file name length is 255 characters
        int fileNameLength = recv(clientSocket, fileNameBuffer, sizeof(fileNameBuffer), 0);
        if (fileNameLength <= 0) {
            std::cerr << "Failed to receive file name." << std::endl;
            return;
        }
        fileNameBuffer[fileNameLength] = '\0';
        std::string fileName(fileNameBuffer);

        // Receive the file size
        long long fileSize;
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0);
        if (bytesReceived != sizeof(fileSize)) {
            std::cerr << "Failed to receive file size." << std::endl;
            return;
        }

        // Open the file for writing
        std::ofstream receivedFile("C:\\KSE IT\\Client Server Concepts\\csc_third\\clientStorage\\" + userName + "\\" + fileName, std::ios::binary);
        if (!receivedFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << fileName << std::endl;
            return;
        }

        // Receive and write file data
        long long totalBytesReceived = 0;
        while (totalBytesReceived < fileSize) {
            int bytesToReceive = std::min(chunkSize, static_cast<int>(fileSize - totalBytesReceived));
            int bytesReceived = recv(clientSocket, fileBuffer, bytesToReceive, 0);
            if (bytesReceived <= 0) {
                std::cerr << "Error while receiving file data." << std::endl;
                return;
            }
            receivedFile.write(fileBuffer, bytesReceived);
            totalBytesReceived += bytesReceived;
        }

        std::cout << "File received: " << fileName << std::endl;

        // Close the file
        receivedFile.close();
    }

    void sendFile(const std::string& filePath) const{
        std::ifstream fileToSend(filePath, std::ios::binary);
        if (!fileToSend.is_open()) {
            std::cerr << "Failed to open the file: " << filePath << std::endl;
            return;
        }

        // Sending filename
        size_t lastSlash = filePath.find_last_of("\\/");
        std::string fileName = filePath.substr(lastSlash + 1);
        send(clientSocket, fileName.c_str(), fileName.length(), 0);

        fileToSend.seekg(0, std::ios::end);
        int fileSize = static_cast<int>(fileToSend.tellg());
        fileToSend.seekg(0, std::ios::beg);
        // Sending num of bytes
        send(clientSocket, reinterpret_cast<const char*>(&fileSize), sizeof(fileSize), 0);

        const int chunkSize = 1024;
        char buffer[chunkSize];

        //Sending data
        while (!fileToSend.eof()) {
            fileToSend.read(buffer, chunkSize);
            int bytesRead = static_cast<int>(fileToSend.gcount());
            send(clientSocket, buffer, bytesRead, 0);
        }
        fileToSend.close();
    }

    void ensureUserFolderExists() {
        std::filesystem::path userFolderPath = "C:\\KSE IT\\Client Server Concepts\\csc_third\\clientStorage\\" + userName;

        if (!std::filesystem::exists(userFolderPath)) {
            std::filesystem::create_directory(userFolderPath);
            std::cout << "User folder created: " << userFolderPath << std::endl;
        }
    }
};

int main() {
    Client client;

    std::this_thread::sleep_for(std::chrono::hours(1));

    return 0;
}