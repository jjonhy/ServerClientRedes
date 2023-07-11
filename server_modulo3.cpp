#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <map>
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
const std::string NICKNAME_COMMAND = "/nickname";
const std::string JOIN_COMMAND = "/join";
const std::string KICK_COMMAND = "/kick";
const std::string MUTE_COMMAND = "/mute";
const std::string UNMUTE_COMMAND = "/unmute";
const std::string WHOIS_COMMAND = "/whois";

std::vector<int> connectedClients;
std::vector<std::string> clientNames;
std::map<std::string, std::vector<int>> channelsNames;
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
    std::string currentChannel;

    // Send welcome message to the client
    std::string welcomeMessage = "Welcome to the chat! Your nickname is " + clientName + ".";
    sendMessage(clientSocket, welcomeMessage);

    // Add client to the connected clients vector
    connectedClients.push_back(clientSocket);
    clientNames.push_back(clientName);

    bool connected = true;
    bool isChannelOwner = false;

    while (connected) {
        // Receive message from the client
        std::string receivedMessage = receiveMessage(clientSocket);
        if (receivedMessage.empty()) {
            std::cout << clientName << " has disconnected." << std::endl;
            break;
        }

        // Check if the client wants to quit
        if (receivedMessage == QUIT_COMMAND) {
            std::cout << clientName << " has left the chat." << std::endl;
            break;
        }

        // Check if the client wants to ping the server
        if (receivedMessage == PING_COMMAND) {
            sendMessage(clientSocket, PONG_MESSAGE);
            continue;
        }

        //Check if the client wants to add Nickname
        if(receivedMessage.rfind(NICKNAME_COMMAND, 0) == 0) {
            std::string newName = receivedMessage.substr(10);
            std::cout << "Client " << clientId << " is now ";
            
            int index = find(clientNames.begin(), clientNames.end(), clientName) - clientNames.begin();
            clientName = newName;
            clientNames[index] = clientName;
            std::cout << clientName << std::endl;  
        }

        //Check if the client wants to join/create a channel
        if(receivedMessage.rfind(JOIN_COMMAND, 0) == 0) {
            std::string channelName = receivedMessage.substr(6);
            //Check if channel exists
            if(channelsNames.find(channelName) == channelsNames.end()){

                channelsNames[channelName].push_back(clientId);
                currentChannel = channelName;
                std::string creationMessage = "Channel " + channelName + " created";
                std::cout << creationMessage << std::endl;  
                sendMessage(clientSocket, creationMessage);
                isChannelOwner = true;

            }else{
                currentChannel = channelName;
                channelsNames[channelName].push_back(clientId);
                std::string connectionMessage = "Connected to the channel: " + channelName;
                sendMessage(clientSocket, connectionMessage);
            }            

        }

        //If user wants to kick another user from the channel
        if(receivedMessage.rfind(KICK_COMMAND, 0) == 0){
            //Checks if he is administrator
            if(isChannelOwner){
                std::string userName = receivedMessage.substr(6);
                
                //Searches for the user in all the clients
                auto flag = find(clientNames.begin(), clientNames.end(), userName);
                if(flag == clientNames.end()){
                    std::string userMessage = "User not found.";
                    sendMessage(clientSocket, userMessage);
                    continue;
                }
                int index =  flag - clientNames.begin();

                //Disconnects the user 
                auto flag2 = find(channelsNames[currentChannel].begin(), channelsNames[currentChannel].end(), connectedClients[index]);
                if(flag2 == channelsNames[currentChannel].end()){
                    std::string userMessage = "User not found.";
                    sendMessage(clientSocket, userMessage);
                    continue;
                }
                int index2 =  flag2 - channelsNames[currentChannel].begin();
                channelsNames[currentChannel][index2] = -1;


                std::string userMessage = "User " + userName + " was kicked.";
                sendMessage(clientSocket, userMessage);

                std::string kickedMessage = "You were kicked of the channel " + currentChannel +" by an administrator.";
                sendMessage(connectedClients[index], kickedMessage);
                continue;
            }else{
                std::string permissionMessage = "This command may only be used by the channel administrator";
                sendMessage(clientSocket, permissionMessage);
                continue;
            }
        }


        //If user wants to mute another user from the channel
        if(receivedMessage.rfind(MUTE_COMMAND, 0) == 0){
            //Checks if he is administrator
            if(isChannelOwner){
                std::string userName = receivedMessage.substr(6);
                
                //Searches for the user in all the clients
                auto flag = find(clientNames.begin(), clientNames.end(), userName);
                if(flag == clientNames.end()){
                    std::string userMessage = "User not found.";
                    sendMessage(clientSocket, userMessage);
                    continue;
                }
                int index =  flag - clientNames.begin();

                std::string userMessage = "User " + userName + " was muted.";
                sendMessage(clientSocket, userMessage);

                std::string mutedMessage = "You were muted on the channel " + currentChannel +" by an administrator.";
                sendMessage(connectedClients[index], mutedMessage);
                continue;
            }else{
                std::string permissionMessage = "This command may only be used by the channel administrator";
                sendMessage(clientSocket, permissionMessage);
                continue;
            }
        }

        //If user wants to unmute another user from the channel
        if(receivedMessage.rfind(UNMUTE_COMMAND, 0) == 0){
            //Checks if he is administrator
            if(isChannelOwner){
                std::string userName = receivedMessage.substr(8);
                
                //Searches for the user in all the clients
                auto flag = find(clientNames.begin(), clientNames.end(), userName);
                if(flag == clientNames.end()){
                    std::string userMessage = "User not found.";
                    sendMessage(clientSocket, userMessage);
                    continue;
                }
                int index =  flag - clientNames.begin();

                std::string userMessage = "User " + userName + " was unmuted.";
                sendMessage(clientSocket, userMessage);

                std::string unmutedMessage = "You were unmuted on the channel " + currentChannel +" by an administrator.";
                sendMessage(connectedClients[index], unmutedMessage);
                continue;
            }else{
                std::string permissionMessage = "This command may only be used by the channel administrator";
                sendMessage(clientSocket, permissionMessage);
                continue;
            }
        }

        //If user wants to get the ip address another user from the channel
        if(receivedMessage.rfind(WHOIS_COMMAND, 0) == 0){
            //Checks if he is administrator
            if(isChannelOwner){
                std::string userName = receivedMessage.substr(6);
                
                //Searches for the user in all the clients
                auto flag = find(clientNames.begin(), clientNames.end(), userName);
                if(flag == clientNames.end()){
                    std::string userMessage = "User not found.";
                    sendMessage(clientSocket, userMessage);
                    continue;
                }
                int index =  flag - clientNames.begin();

                //Pegar o ip aqui, não do cliente que mandou a mensagem mas do alvo userName, que pegamos o  index no vetor com os ID's
                std::string ip = "127.0.0.1";
                std::string userMessage = "User " + userName + " is on IP: " + ip;
                sendMessage(clientSocket, userMessage);

                continue;
            }else{
                std::string permissionMessage = "This command may only be used by the channel administrator";
                sendMessage(clientSocket, permissionMessage);
                continue;
            }
        }
        std::string fullMessage = clientName + ": " + receivedMessage;

        // Send the message to all clients in the same channel
        for (int i = 0; i < channelsNames[currentChannel].size(); i++) {
            
            int destinationSocket = channelsNames[currentChannel][i];
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
