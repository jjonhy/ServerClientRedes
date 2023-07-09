#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

const int BUFFER_SIZE = 4096;
const std::string QUIT_COMMAND = "/quit";
const std::string PING_COMMAND = "/ping";
const std::string PONG_MESSAGE = "pong";

std::vector<int> connectedClients;
std::vector<std::string> clientNames;
std::atomic<bool> exitServer(false);

void sendMessage(int socket, const std::string& message) {
    int messageLength = message.length();
    send(socket, &messageLength, sizeof(messageLength), 0);

    int bytesSent = 0;
    while (bytesSent < messageLength) {
        int remainingBytes = messageLength - bytesSent;
        int bytesToSend = std::min(remainingBytes, BUFFER_SIZE);
        send(socket, message.c_str() + bytesSent, bytesToSend, 0);
        bytesSent += bytesToSend;
    }
}

std::string receiveMessage(int socket) {
    int messageLength;
    recv(socket, &messageLength, sizeof(messageLength), 0);

    char buffer[BUFFER_SIZE];
    std::string message;

    int bytesRead = 0;
    while (bytesRead < messageLength) {
        int remainingBytes = messageLength - bytesRead;
        int bytesToReceive = std::min(remainingBytes, BUFFER_SIZE - 1);
        int receivedBytes = recv(socket, buffer, bytesToReceive, 0);
        if (receivedBytes <= 0) {
            break;
        }

        buffer[receivedBytes] = '\0';
        message += buffer;
        bytesRead += receivedBytes;
    }

    return message;
}

void clientThread(int clientSocket) {
    int clientId = clientSocket;
    std::string clientName = "Client " + std::to_string(clientId);

    // Send welcome message to the client
    std::string welcomeMessage = "Welcome to the chat! Your nickname is " + clientName + ".";
    sendMessage(clientSocket, welcomeMessage);

    // Add client to the connected clients vector
    connectedClients.push_back(clientSocket);
    clientNames.push_back(clientName);

    bool connected = true;

    while (connected) {
        // Receive message from the client
        std::string receivedMessage = receiveMessage(clientSocket);
        if (receivedMessage.empty()) {
            std::cout << "Client " << clientId << " has disconnected." << std::endl;
            break;
        }

        // Check if the client wants to quit
        if (receivedMessage == QUIT_COMMAND) {
            std::cout << "Client " << clientId << " has left the chat." << std::endl;
            break;
        }

        // Check if the client wants to ping the server
        if (receivedMessage == PING_COMMAND) {
            sendMessage(clientSocket, PONG_MESSAGE);
            continue;
        }

        std::string fullMessage = clientName + ": " + receivedMessage;

        // Send the message to all connected clients
        for (int i = 0; i < connectedClients.size(); i++) {
            int destinationSocket = connectedClients[i];
            sendMessage(destinationSocket, fullMessage);
        }
    }

    // Remove client from the connected clients vector
    auto it = std::find(connectedClients.begin(), connectedClients.end(), clientSocket);
    if (it != connectedClients.end()) {
        connectedClients.erase(it);
        clientNames.erase(clientNames.begin() + std::distance(connectedClients.begin(), it));
    }

    // Close the client socket
    close(clientSocket);
}

void signalHandler(int signum) {
    if (signum == SIGINT) {
        std::cout << "Server interrupted. Closing connections..." << std::endl;
        exitServer = true;

        // Close all client sockets
        for (int clientSocket : connectedClients) {
            close(clientSocket);
        }
    }
}

int main() {
    signal(SIGINT, signalHandler);

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }

    // Set up server address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(12345);  // Choose a suitable port number

    // Bind the socket to the server address
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Failed to bind socket." << std::endl;
        return 1;
    }

    // Start listening for incoming connections
    listen(serverSocket, 3);

    std::cout << "Waiting for incoming connections..." << std::endl;

    while (!exitServer) {
        // Accept a connection from a client
        int clientSocket;
        sockaddr_in clientAddress;
        socklen_t clientAddressLength = sizeof(clientAddress);
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLength);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept connection." << std::endl;
            return 1;
        }

        std::cout << "Client connected. Client ID: " << clientSocket << std::endl;

        // Start a new thread for each client
        std::thread clientThreadObj(clientThread, clientSocket);
        clientThreadObj.detach();
    }

    // Close the server socket
    close(serverSocket);

    return 0;
}
