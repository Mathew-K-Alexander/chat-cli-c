#include "socketutil.h"

int createTCPIpv4Socket() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);

    // Allow address reuse (needed to fix "Address already in use")
    int opt = 1;
    setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    return socketFD;
}

struct sockaddr_in* createIPv4Address(const char* ip, int port) {
    struct sockaddr_in* address = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    if (!address) return NULL;

    memset(address, 0, sizeof(struct sockaddr_in));
    address->sin_family = AF_INET;
    address->sin_port = htons(port);

    if (strlen(ip) == 0) {
        address->sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        inet_pton(AF_INET, ip, &address->sin_addr);
    }

    return address;
}
