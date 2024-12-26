// server/include/chat.h

#ifndef SERVER_CHAT_H
#define SERVER_CHAT_H

#include "../include/db.h"

// Hàm xử lý tin nhắn 1-1
void handle_private_message(DBConnection *db, int sender_id, const char *recipient_username, const char *message, int client_sock);

// Hàm gửi tin nhắn tới người nhận
void send_message_to_user(int recipient_sock, const char *message);

#endif // SERVER_CHAT_H
