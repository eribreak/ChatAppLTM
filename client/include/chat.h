// client/include/chat.h

#ifndef CHAT_H
#define CHAT_H

#include <stdbool.h>

// Hàm khởi tạo kết nối chat
int init_chat(const char *server_ip, int port, const char *username);

// Hàm gửi tin nhắn 1-1
void send_private_message(int sock, const char *recipient, const char *message);

// Hàm nhận tin nhắn
void *receive_messages(void *arg);

// Hàm xử lý lệnh chat
void handle_chat(int sock, const char *username);

#endif // CHAT_H
