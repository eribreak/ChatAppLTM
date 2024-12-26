// client/src/utils.c

#include "utils.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

void handle_error(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

int send_all(int sock, const void *buffer, size_t length) {
    size_t totalSent = 0;
    const char *ptr = buffer;
    while (totalSent < length) {
        ssize_t bytesSent = send(sock, ptr + totalSent, length - totalSent, 0);
        if (bytesSent <= 0) {
            return -1; // Lỗi khi gửi
        }
        totalSent += bytesSent;
    }
    return 0; // Gửi thành công
}

int recv_all(int sock, void *buffer, size_t length) {
    size_t totalReceived = 0;
    char *ptr = buffer;
    while (totalReceived < length) {
        ssize_t bytesReceived = recv(sock, ptr + totalReceived, length - totalReceived, 0);
        if (bytesReceived <= 0) {
            return -1; // Lỗi hoặc kết nối đóng
        }
        totalReceived += bytesReceived;
    }
    return totalReceived;
}

uint64_t htonll(uint64_t value) {
    int num = 1;
    if (*(char *)&num == 1) {
        // Little endian
        uint32_t high_part = htonl((uint32_t)(value >> 32));
        uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFFLL));
        return (((uint64_t)high_part) << 32) | low_part;
    } else {
        // Big endian
        return value;
    }
}

uint64_t ntohll(uint64_t value) {
    int num = 1;
    if (*(char *)&num == 1) {
        // Little endian
        uint32_t high_part = ntohl((uint32_t)(value >> 32));
        uint32_t low_part = ntohl((uint32_t)(value & 0xFFFFFFFFLL));
        return (((uint64_t)high_part) << 32) | low_part;
    } else {
        // Big endian
        return value;
    }
}
