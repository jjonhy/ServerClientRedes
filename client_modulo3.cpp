#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

const int BUFFER_SIZE = 4096;
bool joinedChannel = false;
bool isMute = false;
const std::string QUIT_COMMAND = "/quit";
const std::string PING_COMMAND = "/ping";
const std::string PONG_MESSAGE = "pong";

const std::string NICKNAME_COMMAND = "/nickname";
const std::string JOIN_COMMAND = "/join";


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

void receiveThread(int serverSocket) {
    while (true) {
        std::string receivedMessage = receiveMessage(serverSocket);
        if (receivedMessage.empty()) {
            std::cout << "Disconnected from the server." << std::endl;
            break;
        }

        std::cout << receivedMessage << std::endl;

        if(receivedMessage.rfind("You were kicked", 0) == 0){
            joinedChannel = false;
        }

        if(receivedMessage.rfind("You were muted", 0) == 0){
            isMute = true;
        }        

        if(receivedMessage.rfind("You were unmuted", 0) == 0){
            isMute = false;
        }   
    }
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
    serverAddress.sin_port = htons(12345);  // Use the same port number as the server
    if (inet_pton(AF_INET, "127.0.0.1", &(serverAddress.sin_addr)) <= 0) {
        std::cerr << "Invalid address or address not supported." << std::endl;
        return 1;
    }

    // Connect to the server
    if (connect(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Connection failed." << std::endl;
        return 1;
    }

    // Receive the welcome message from the server
    std::string welcomeMessage = receiveMessage(serverSocket);
    std::cout << welcomeMessage << std::endl;

    // Create receive thread
    std::thread receiveThreadObj(receiveThread, serverSocket);

    bool connected = true;
    bool sentConnectCommand = false;
    bool sentNicknameCommand = false;


    // Read user input and send messages
    std::string userInput;
    while (connected) {
        std::getline(std::cin, userInput);

        // Check if the user is connected and has sent the connect command
        if (!sentConnectCommand) {
            if (userInput == "/connect") {
                sentConnectCommand = true;
                sendMessage(serverSocket, userInput);
                std::cout << "Connected to the server. You can now register choosing a nickname with /nickname." << std::endl;
                continue;
            } else {
                std::cout << "Please enter the /connect command to establish the connection." << std::endl;
                continue;
            }
        }

        //Check if the user has entered a nickname
        if (!sentNicknameCommand && sentConnectCommand) {
            //Check if user wants to add a nickname
            if(userInput.rfind(NICKNAME_COMMAND, 0) == 0){
                sentNicknameCommand = true; 
                std::cout << "Nickname changed, you can now join a channel with /join" << std::endl;
                sendMessage(serverSocket, userInput);
                continue;
            }else {
                std::cout << "Please enter the /nickname command to register." << std::endl;
                continue;
            }
        }

        //Check if the user has joined a channel
        if (!joinedChannel && sentNicknameCommand && sentConnectCommand) {
            if(userInput.rfind(JOIN_COMMAND, 0) == 0){
                joinedChannel = true; 
                sendMessage(serverSocket, userInput);
                continue;
            }else {
                std::cout << "Please join a channel." << std::endl;
                continue;
            }
        }


        // Check if the user wants to quit
        if (userInput == QUIT_COMMAND) {
            std::cout << "Disconnecting from the server..." << std::endl;
            sendMessage(serverSocket, userInput);
            break;
        }

        // Check if the user wants to ping the server
        if (userInput == PING_COMMAND) {
            std::cout << "Pinging the server..." << std::endl;
            sendMessage(serverSocket, userInput);
            continue;
        }



        // Send the user's message to the server
        if(joinedChannel && sentNicknameCommand && sentConnectCommand && !isMute) sendMessage(serverSocket, userInput);
    }

    // Close the server socket
    close(serverSocket);

    // Wait for the receive thread to finish
    receiveThreadObj.join();

    return 0;
}
