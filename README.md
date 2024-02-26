# Chat Application
This application is a simple client-server application that enables clients to connect to a server, join chat rooms, exchange text messages, and send files to one another. The application is built using C++ and relies on the Winsock library for network communication. Below is a detailed protocol description and explanation of how the server and client components work, including the byte sizes for data sent across the network.

## General Overview
### Initialization:
- Server: Initializes Winsock, creates a socket, binds it to an address, and listens for incoming connections on a specified port.
- Client: Initializes Winsock, creates a socket, and connects to the server using the server's IP address and port number.

### Client-Side Operations:
- Upon successfully connecting to the server, the client enters a chat room ID and their name, which are sent to the server.
- The client can then send text messages or commands (e.g., to send a file or exit the chat room).
- A separate thread is created for receiving messages from the server.

### Server-Side Operations:
- Accepts incoming connections and handles each client in a separate thread.
- Receives the client's name and chat room ID, adding the client to the specified chat room.
- Forwards text messages to all other clients in the same chat room.
- Handles file transfer requests and notifications.

## Messaging Protocol
### Sending name and room ID
When we start our execution, we send the name (bytes: depends on the length of the name) and the room number (bytes: 1)

### User Input:
- The client enters a text message into the console. This action is performed within a loop that continuously prompts the user for input. Amount of bytes transmitted depends on the massage length.

### Message Sending:
- The entered message is sent to the server using the send() function. Before sending, the message is stored as a string. The send() function takes the socket, the message, its length, and flags (typically set to 0) as parameters.
- Data Sent: The message string's bytes plus a null terminator if applicable, depending on how string lengths are managed in transmission.

### Server Reception:
- The server, upon receiving a message from a client, reads the data into a buffer using recv(). It determines the message's end or length based on predefined protocols (e.g., null-terminated strings, fixed-length messages, or including a length prefix). 
- After reading data, server puts message in the message queue. 

### Message Distribution:
- Later, server takes messages from the queue one by one.
- The server then identifies the chat room associated with the sending client and iterates over all clients in that room, excluding the sender.
- It forwards the received message to each client using send(), replicating the message across the chat room. The length of the message distributed depends on the name of sender and the message itseld. So the amount of bytes will be like "length of name" + "length of message" + 2 - "2" stands separator between the name and the message.

### Client Reception:
- Each client in the room receives the message in their respective recv() loop, running in a dedicated thread to handle incoming data without blocking the main thread.

### Displaying the Message:
- Upon receiving a message, the client processes and displays it in the console, making it visible to the user.

## File Receiving Protocol
### File Send Request:

- A client initiates a file transfer by sending a specific command (e.g., "SEND") followed by the file path or identifier.
- The server gets file from the client.
1. File Name and Size: First, the client sends the file name (bytes: length of the name) and its size (bytes: 4 bytes) to the server. This is server for the client to prepare for file reception, including allocating space and opening a file stream for writing.
2. Chunked Data Transfer: The file is transmitted in chunks, often 1024 bytes at a time, until the entire file is sent. The client reads from the file and sends each chunk sequentially. The server receives each chunk, writing it to the specified file location as it arrives.
- Then it parses this command and prepares to handle the file transfer to the clients.

### Server Notification to Other Clients:
- The server sends a notification to all other clients in the room, indicating an incoming file transfer. This notification includes the file name and size and asks if they accept the file. The length of this notification depends on the two variables: file name and its size.

### Client Acceptance:
- Each client responds with either acceptance ("ACCEPT") or refusal ("NO"). This decision may trigger different flows, such as proceeding with file reception or skipping the transfer.

### File Data Transmission:
- Upon receiving confirmation to proceed, the server begins the file transmission process in the same way as the client sends the file to the temporary server storage. The only difference is that the size of the file is 8 bytes length.

### Completion and Cleanup:
- After all chunks have been transmitted, the server and client perform necessary cleanup actions. This includes closing file streams and, on the server side, potentially deleting the file or marking it as sent.
- The server then resumes listening for further commands or messages, and the client continues to await user input or incoming data.

### Joining & Messages
![Example Image](/images/join_and_messages.png)
#### Joining Room
```cpp
void addClientToRoom(SOCKET clientSocket, const std::string &roomID, const std::string &clientName) {
        std::lock_guard<std::mutex> lock(roomMutex);
        rooms[roomID].push_back({clientSocket, clientName});
        roomCounters[roomID]++;
        std::cout << "Client " << clientName << " added to room " << roomID << std::endl;

        std::string message = clientName + " has joined the room.";
        for (const auto& client : rooms[roomID]) {
            if (client.socket != clientSocket) {
                send(client.socket, message.c_str(), message.length(), 0);
            }
        }
    }
```

#### Messaging
```cpp
 void addMessageToQueue(const std::string& roomID, const std::string& clientName, const std::string& message) {
        std::lock_guard<std::mutex> lock(queueMutex);
        messageQueue.push({roomID, clientName, message});
        queueCondition.notify_one();
    }

    void processMessageQueue() {
        while (true) {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCondition.wait(lock, [this]() { return !messageQueue.empty(); });
            QueuedMessage msg = messageQueue.front();
            messageQueue.pop();
            lock.unlock();
            sendMessageToRoom(msg.roomID, msg.clientName, msg.message.c_str(), msg.message.length());
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
```

### Changing Room
![Example Image](/images/room_change.png)
```cpp
if (message.find("CHANGE ") == 0) {
                std::string newRoomID = message.substr(7);
                removeClientFromRoom(clientSocket, roomID);
                roomID = newRoomID;
                addClientToRoom(clientSocket, roomID, clientName);
                ...
void addClientToRoom(SOCKET clientSocket, const std::string &roomID, const std::string &clientName) {
        std::lock_guard<std::mutex> lock(roomMutex);
        rooms[roomID].push_back({clientSocket, clientName});
        roomCounters[roomID]++;
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
            roomCounters[roomID]--;
            std::cout << "Client " << clientName << " removed from room " << roomID << std::endl;

            std::string message = clientName + " has left the room.";
            for (const auto& client : clients) {
                send(client.socket, message.c_str(), message.length(), 0);
            }
        }
    }
```

### File Sending
![Example Image](/images/file_sending.png)
```cpp
void sendFile(SOCKET clientSocket, const std::string& fileName,  std::uintmax_t fileSize){
        std::ifstream fileToSend("C:\\KSE IT\\Client Server Concepts\\csc_third\\serverStorage\\" + fileName, std::ios::binary);
        if (!fileToSend.is_open()) {
            std::cerr << "Failed to open file for reading: " << fileName << std::endl;
            return;
        }

        const int chunkSize = 1024;
        char fileBuffer[chunkSize];

        send(clientSocket, fileName.c_str(), fileName.length(), 0);
        send(clientSocket, reinterpret_cast<const char*>(&fileSize), sizeof(fileSize), 0);

        // Check if there's only one client in the room
        std::string roomID = getClientRoomID(clientSocket);
        int remainingClients = roomCounters[roomID] - 1; // Subtract 1 for the sender
        while (!fileToSend.eof() && remainingClients > 0) {
            fileToSend.read(fileBuffer, chunkSize);
            int bytesRead = static_cast<int>(fileToSend.gcount());
            send(clientSocket, fileBuffer, bytesRead, 0);
            remainingClients--;
        }

        std::cout << "File sent to client"  << std::endl;

        fileToSend.close();

        // Remove the file from storage if it has been sent to all clients
        if (remainingClients == 0) {
            std::filesystem::remove("C:\\KSE IT\\Client Server Concepts\\csc_third\\serverStorage\\" + fileName);
            std::cout << "File removed from storage: " << fileName << std::endl;
        }
    }
```

### Exiting
![Example Image](/images/exiting.png)
```cpp
else if (message.find("EXIT") == 0) {
                removeClientFromRoom(clientSocket, roomID);
                closesocket(clientSocket);
            }
```

### Sequence for Messages
![Example Image](/images/sequence_messages.png)

### Sequence for Files
![Example Image](/images/sequence_files.png)




