#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#endif

#include "socketutil.h"



void startListeningAndPrintMessagesOnNewThread(int fd);
void* listenAndPrint(void* arg);
void readConsoleEntriesAndSendToServer(int socketFD);

#ifdef _WIN32
#include <stdlib.h>
#include <stdio.h>

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    if (!lineptr || !n || !stream) return -1;

    size_t pos = 0;
    int c;

    if (*lineptr == NULL || *n == 0) {
        *n = 128;
        *lineptr = malloc(*n);
        if (*lineptr == NULL) return -1;
    }

    while ((c = fgetc(stream)) != EOF) {
        if (pos + 1 >= *n) {
            *n *= 2;
            char *new_ptr = realloc(*lineptr, *n);
            if (!new_ptr) return -1;
            *lineptr = new_ptr;
        }

        (*lineptr)[pos++] = c;
        if (c == '\n') break;
    }

    if (pos == 0) return -1;

    (*lineptr)[pos] = '\0';
    return pos;
}
#endif

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    int socketFD = createTCPIpv4Socket();
    struct sockaddr_in* address = createIPv4Address("127.0.0.1", 2000);

    if (connect(socketFD, (struct sockaddr*)address, sizeof(*address)) != 0) {
        perror("Connection failed");
        return 1;
    }

    printf("Connected to server.\n");

    // --- Send password ---
    char password[128];
    printf("Enter password to authenticate: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0; // Remove newline

    send(socketFD, password, strlen(password), 0);

    // --- Wait for auth response ---
    char response[128];
    int received = recv(socketFD, response, sizeof(response) - 1, 0);
    if (received <= 0) {
        printf("Failed to receive authentication response.\n");
        return 1;
    }
    response[received] = '\0';

    if (strcmp(response, "AUTH_OK") != 0) {
        printf("Authentication failed: %s\n", response);
#ifdef _WIN32
        closesocket(socketFD);
        WSACleanup();
#else
        close(socketFD);
#endif
        return 1;
    }

    printf("Authentication successful.\n");

    startListeningAndPrintMessagesOnNewThread(socketFD);
    readConsoleEntriesAndSendToServer(socketFD);

#ifdef _WIN32
    closesocket(socketFD);
    WSACleanup();
#else
    close(socketFD);
#endif
    free(address);

    return 0;
}


void readConsoleEntriesAndSendToServer(int socketFD) {
    char* name = NULL;
    size_t nameSize = 0;
    printf("Please enter your name:\n");
    ssize_t nameCount = getline(&name, &nameSize, stdin);
    if (nameCount > 1)
        name[nameCount - 1] = '\0';

    char* line = NULL;
    size_t lineSize = 0;
    printf("Type and we will send (type 'exit' to quit):\n");

    char buffer[1024];

    while (1) {
        ssize_t charCount = getline(&line, &lineSize, stdin);
        if (charCount > 1) {
            line[charCount - 1] = '\0';
        } else {
            continue;
        }

        if (strcmp(line, "exit") == 0)
            break;

        snprintf(buffer, sizeof(buffer), "%s: %s", name, line);
        send(socketFD, buffer, strlen(buffer), 0);
    }

    free(name);
    free(line);
}

void startListeningAndPrintMessagesOnNewThread(int socketFD) {
    pthread_t id;
    int* socketFDPtr = malloc(sizeof(int));
    *socketFDPtr = socketFD;
    pthread_create(&id, NULL, listenAndPrint, socketFDPtr);
    pthread_detach(id);
}

void* listenAndPrint(void* arg) {
    int socketFD = *(int*)arg;
    free(arg);

    char buffer[1024];

    while (1) {
        int amountReceived = recv(socketFD, buffer, sizeof(buffer) - 1, 0);

        if (amountReceived > 0) {
            buffer[amountReceived] = '\0';
            printf("Response was: %s\n", buffer);
        }

        if (amountReceived <= 0)
            break;
    }

#ifdef _WIN32
    closesocket(socketFD);
#else
    close(socketFD);
#endif
    return NULL;
}
