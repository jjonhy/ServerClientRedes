#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>

const int BUFFER_SIZE = 4096;

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
    while (true) {
        std::string receivedMessage = receiveMessage(clientSocket);
        if (receivedMessage.empty()) {
            std::cout << "Connection closed by the client." << std::endl;
            break;
        }

        std::cout << "Client: " << receivedMessage << std::endl;

        // Send a response back to the client
        std::string response;
        std::cout << "Server: ";
        std::getline(std::cin, response);
        sendMessage(clientSocket, response);
    }

    // Close the client socket
    close(clientSocket);
}

int main() {
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

    while (true) {
        // Accept a connection from a client
        int clientSocket;
        sockaddr_in clientAddress;
        socklen_t clientAddressLength = sizeof(clientAddress);
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLength);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept connection." << std::endl;
            return 1;
        }

        std::cout << "Client connected." << std::endl;

        // Start a new thread for each client
        std::thread clientThreadObj(clientThread, clientSocket);
        clientThreadObj.detach();
    }

    // Close the server socket
    close(serverSocket);

    return 0;
}
