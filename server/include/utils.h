// server/include/utils.h

#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include <stdio.h>
#include <stdint.h>

// Hàm xử lý lỗi
void handle_error(const char *message);

// Hàm gửi dữ liệu toàn bộ
int send_all(int sock, const void *buffer, size_t length);

// Hàm nhận dữ liệu toàn bộ
int recv_all(int sock, void *buffer, size_t length);

// Hàm chuyển đổi 64-bit từ host to network
uint64_t htonll(uint64_t value);

// Hàm chuyển đổi 64-bit từ network to host
uint64_t ntohll(uint64_t value);

// Hàm gửi phản hồi đến client
void send_response(int sock, const char *message);

#endif // SERVER_UTILS_H
