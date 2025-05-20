#define SERVER_PASSWORD "secret123"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "socketutil.h"

struct AcceptedSocket {
    int acceptedSocketFD;
    struct sockaddr_in address;
    int error;
    bool acceptedSuccessfully;
};

struct AcceptedSocket* acceptIncomingConnection(int serverSocketFD);
void* receiveAndPrintIncomingData(void* arg);
void startAcceptingIncomingConnections(int serverSocketFD);
void receiveAndPrintIncomingDataOnSeparateThread(struct AcceptedSocket* pSocket);
void sendReceivedMessageToTheOtherClients(char* buffer, int socketFD);

struct AcceptedSocket acceptedSockets[10];
int acceptedSocketsCount = 0;

void startAcceptingIncomingConnections(int serverSocketFD) {
    while (true) {
        struct AcceptedSocket* clientSocket = acceptIncomingConnection(serverSocketFD);
        if (clientSocket && clientSocket->acceptedSuccessfully) {
            //acceptedSockets[acceptedSocketsCount++] = *clientSocket;
            //receiveAndPrintIncomingDataOnSeparateThread(clientSocket);
            char passwordBuffer[128] = {0};
            int bytesReceived = recv(clientSocket->acceptedSocketFD, passwordBuffer, sizeof(passwordBuffer) - 1, 0);
            if (bytesReceived <= 0) {
                closesocket(clientSocket->acceptedSocketFD);
                free(clientSocket);
                continue;
            }
            passwordBuffer[bytesReceived] = '\0';

            if (strcmp(passwordBuffer, SERVER_PASSWORD) != 0) {
                const char* msg = "AUTH_FAILED";
                send(clientSocket->acceptedSocketFD, msg, strlen(msg), 0);
                closesocket(clientSocket->acceptedSocketFD);
                free(clientSocket);
                printf("Client failed authentication.\n");
                continue;
            } else {
                const char* msg = "AUTH_OK";
                send(clientSocket->acceptedSocketFD, msg, strlen(msg), 0);
            }

            acceptedSockets[acceptedSocketsCount++] = *clientSocket;
            receiveAndPrintIncomingDataOnSeparateThread(clientSocket);

        } else {
            perror("Failed to accept connection");
            free(clientSocket);
        }
    }
}

void receiveAndPrintIncomingDataOnSeparateThread(struct AcceptedSocket* pSocket) {
    pthread_t id;
    int* socketFDPtr = malloc(sizeof(int));
    *socketFDPtr = pSocket->acceptedSocketFD;

    pthread_create(&id, NULL, receiveAndPrintIncomingData, socketFDPtr);
    pthread_detach(id);
}

void* receiveAndPrintIncomingData(void* arg) {
    int socketFD = *(int*)arg;
    free(arg);

    char buffer[1024];

    while (1) {
        int amountReceived = recv(socketFD, buffer, sizeof(buffer) - 1, 0);
        if (amountReceived > 0) {
            buffer[amountReceived] = '\0';
            printf("%s\n", buffer);
            sendReceivedMessageToTheOtherClients(buffer, socketFD);
        } else {
            break;
        }
    }

#ifdef _WIN32
    closesocket(socketFD);
#else
    close(socketFD);
#endif
    return NULL;
}

void sendReceivedMessageToTheOtherClients(char* buffer, int socketFD) {
    for (int i = 0; i < acceptedSocketsCount; i++) {
        if (acceptedSockets[i].acceptedSocketFD != socketFD) {
            send(acceptedSockets[i].acceptedSocketFD, buffer, strlen(buffer), 0);
        }
    }
}

struct AcceptedSocket* acceptIncomingConnection(int serverSocketFD) {
    struct sockaddr_in clientAddress;
    socklen_t clientAddressSize = sizeof(struct sockaddr_in);

    int clientSocketFD = accept(serverSocketFD, (struct sockaddr*)&clientAddress, &clientAddressSize);

    struct AcceptedSocket* acceptedSocket = malloc(sizeof(struct AcceptedSocket));
    if (!acceptedSocket) {
        perror("Malloc failed for AcceptedSocket");
        return NULL;
    }

    acceptedSocket->address = clientAddress;
    acceptedSocket->acceptedSocketFD = clientSocketFD;
    acceptedSocket->acceptedSuccessfully = clientSocketFD >= 0;

    if (!acceptedSocket->acceptedSuccessfully)
        acceptedSocket->error = clientSocketFD;

    return acceptedSocket;
}

int main() {
    int serverSocketFD = createTCPIpv4Socket();
    struct sockaddr_in* serverAddress = createIPv4Address("", 2000);

    int result = bind(serverSocketFD, (const struct sockaddr*)serverAddress, sizeof(*serverAddress));
    if (result == 0)
        printf("Socket was bound successfully\n");
    else {
        perror("Bind failed");
        return 1;
    }

    int listenResult = listen(serverSocketFD, 10);
    if (listenResult != 0) {
        perror("Listen failed");
        return 1;
    }

    printf("Server listening on port 2000...\n");
    startAcceptingIncomingConnections(serverSocketFD);

#ifdef _WIN32
    closesocket(serverSocketFD);
    WSACleanup();
#else
    close(serverSocketFD);
#endif
    free(serverAddress);

    return 0;
}
