#include <ncurses.h>
#include <time.h>  

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#define MAX_MESSAGES 100
#define MAX_LEN 1024

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#endif

#include "socketutil.h"

short client_color = 1; 
pthread_mutex_t message_mutex = PTHREAD_MUTEX_INITIALIZER;

char messages[MAX_MESSAGES][MAX_LEN];
int message_count = 0;
WINDOW *inputwin;

void startListeningAndPrintMessagesOnNewThread(int socketFD, const char* name);
void* listenAndPrint(void* arg);
void readConsoleEntriesAndSendToServer(int socketFD, const char* name);

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

    char* name = NULL;
    size_t nameSize = 0;
    printf("Please enter your name:\n");
    ssize_t nameCount = getline(&name, &nameSize, stdin);
    if (nameCount > 1)
        name[nameCount - 1] = '\0';

    startListeningAndPrintMessagesOnNewThread(socketFD, name);
    readConsoleEntriesAndSendToServer(socketFD, name);

#ifdef _WIN32
    closesocket(socketFD);
    WSACleanup();
#else
    close(socketFD);
#endif
    free(address);
    free(name);

    return 0;
}


void draw_messages() {
    clear();
    pthread_mutex_lock(&message_mutex);
    for (int i = 0; i < message_count; i++) {
        char* colon = strchr(messages[i], ':');
        if (colon != NULL) {
            int name_len = colon - messages[i];
            attron(COLOR_PAIR(client_color));
            mvprintw(i, 0, "%.*s", name_len, messages[i]); // Name
            attroff(COLOR_PAIR(client_color));
            printw("%s", colon); // Message text
        } else {
            mvprintw(i, 0, "%s", messages[i]); // Fallback
        }
    }
    pthread_mutex_unlock(&message_mutex);
    refresh();
}


void readConsoleEntriesAndSendToServer(int socketFD, const char* name) {
    initscr();

    start_color();
    srand(time(NULL));
    client_color = 1 + rand() % 6; 
    init_pair(1, COLOR_RED,     COLOR_BLACK);
    init_pair(2, COLOR_GREEN,   COLOR_BLACK);
    init_pair(3, COLOR_YELLOW,  COLOR_BLACK);
    init_pair(4, COLOR_BLUE,    COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(6, COLOR_CYAN,    COLOR_BLACK);

    cbreak();
    noecho();
    keypad(stdscr, TRUE);     
    curs_set(1);              

    int row, col;
    getmaxyx(stdscr, row, col);
    inputwin = newwin(3, col, row - 3, 0); 
    box(inputwin, 0, 0);
    wrefresh(inputwin);

    nodelay(inputwin, TRUE); 

    char input[MAX_LEN];
    int pos = 0;
    int ch;

    input[0] = '\0';

    while (1) {
        draw_messages();

        // Redraw input line
        mvwprintw(inputwin, 1, 1, "You: %s", input);
        wclrtoeol(inputwin);
        wrefresh(inputwin);

        ch = wgetch(inputwin);

        if (ch == ERR) {
            // No key pressed yet
            napms(50); // Sleep briefly to reduce CPU usage
            continue;
        }

        if (ch == '\n') {
            input[pos] = '\0';

            if (strcmp(input, "exit") == 0)
                break;

            char buffer[MAX_LEN];
            snprintf(buffer, sizeof(buffer), "%.100s: %.900s", name, input);
            send(socketFD, buffer, strlen(buffer), 0);

            pthread_mutex_lock(&message_mutex);
            snprintf(messages[message_count++ % MAX_MESSAGES], MAX_LEN, "You: %.1000s", input);
            pthread_mutex_unlock(&message_mutex);

            input[0] = '\0';
            pos = 0;
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            if (pos > 0) {
                pos--;
                input[pos] = '\0';
            }
        } else if (ch >= 32 && ch <= 126 && pos < MAX_LEN - 1) {
            input[pos++] = ch;
            input[pos] = '\0';
        }
    }

    endwin();
}

void startListeningAndPrintMessagesOnNewThread(int socketFD, const char* name) {
    pthread_t id;
    struct ListenArgs {
        int socketFD;
        char name[128];
    };
    struct ListenArgs* args = malloc(sizeof(struct ListenArgs));
    args->socketFD = socketFD;
    strncpy(args->name, name, sizeof(args->name));
    pthread_create(&id, NULL, listenAndPrint, args);
    pthread_detach(id);
}

void* listenAndPrint(void* arg) {
    struct ListenArgs {
        int socketFD;
        char name[128];
    };
    struct ListenArgs* args = (struct ListenArgs*)arg;
    int socketFD = args->socketFD;
    free(arg);
    char buffer[1024];

    while (1) {
        int amountReceived = recv(socketFD, buffer, sizeof(buffer) - 1, 0);
        if (amountReceived > 0) {
            buffer[amountReceived] = '\0';
            pthread_mutex_lock(&message_mutex);
            snprintf(messages[message_count++ % MAX_MESSAGES], MAX_LEN, "%s", buffer);
            pthread_mutex_unlock(&message_mutex);
        } else break;
    }

#ifdef _WIN32
    closesocket(socketFD);
#else
    close(socketFD);
#endif
    return NULL;
}

