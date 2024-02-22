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
### User Input:
- The client enters a text message into the console. This action is performed within a loop that continuously prompts the user for input.

### Message Sending:
- The entered message is sent to the server using the send() function. Before sending, the message is stored as a string. The send() function takes the socket, the message, its length, and flags (typically set to 0) as parameters.
- Data Sent: The message string's bytes plus a null terminator if applicable, depending on how string lengths are managed in transmission.

### Server Reception:
- The server, upon receiving a message from a client, reads the data into a buffer using recv(). It determines the message's end or length based on predefined protocols (e.g., null-terminated strings, fixed-length messages, or including a length prefix). 
- After reading data, server puts message in the message queue. 

### Message Distribution:
- Later, server takes messages from the queue one by one.
- The server then identifies the chat room associated with the sending client and iterates over all clients in that room, excluding the sender.
- It forwards the received message to each client using send(), replicating the message across the chat room.

### Client Reception:
- Each client in the room receives the message in their respective recv() loop, running in a dedicated thread to handle incoming data without blocking the main thread.

### Displaying the Message:
- Upon receiving a message, the client processes and displays it in the console, making it visible to the user.

## File Receiving Protocol
### File Send Request:

- A client initiates a file transfer by sending a specific command (e.g., "SEND") followed by the file path or identifier.
- The server gets file from the client.
1. File Name and Size: First, the client sends the file name and its size to the server. This is server for the client to prepare for file reception, including allocating space and opening a file stream for writing.
2. Chunked Data Transfer: The file is transmitted in chunks, often 1024 bytes at a time, until the entire file is sent. The client reads from the file and sends each chunk sequentially. The server receives each chunk, writing it to the specified file location as it arrives.
- Then it parses this command and prepares to handle the file transfer to the clients.

### Server Notification to Other Clients:
- The server sends a notification to all other clients in the room, indicating an incoming file transfer. This notification includes the file name and size and asks if they accept the file.

### Client Acceptance:
- Each client responds with either acceptance ("ACCEPT") or refusal ("NO"). This decision may trigger different flows, such as proceeding with file reception or skipping the transfer.

### File Data Transmission:
- Upon receiving confirmation to proceed, the server begins the file transmission process in the same way as client send file to the temporary server storage.

### Completion and Cleanup:
- After all chunks have been transmitted, the server and client perform necessary cleanup actions. This includes closing file streams and, on the server side, potentially deleting the file or marking it as sent.
- The server then resumes listening for further commands or messages, and the client continues to await user input or incoming data.

### Joining & Messages
![Example Image](/images/join_and_messages.png)

### Changing Room
![Example Image](/images/room_change.png)

### File Sending
![Example Image](/images/file_sending.png)

### Exiting
![Example Image](/images/exiting.png)

### Sequence for Messages
![Example Image](/images/sequence_messages.png)

### Sequence for Files
![Example Image](/images/sequence_files.png)




