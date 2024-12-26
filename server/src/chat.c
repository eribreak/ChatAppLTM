// server/src/chat.c

#include "../include/chat.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BUFFER_SIZE 1024

void handle_private_message(DBConnection *db, int sender_id, const char *recipient_username, const char *message, int client_sock) {
    // Tìm user_id của người nhận
    char query[BUFFER_SIZE];
    snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s'", recipient_username);
    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL) {
        send_response(client_sock, "Recipient not found.\n");
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL) {
        send_response(client_sock, "Recipient not found.\n");
        mysql_free_result(res);
        return;
    }

    int recipient_id = atoi(row[0]);
    mysql_free_result(res);

    // Lưu tin nhắn vào cơ sở dữ liệu
    snprintf(query, sizeof(query),
             "INSERT INTO messages (sender_id, receiver_id, message) VALUES (%d, %d, '%s')",
             sender_id, recipient_id, message);
    if (execute_query(db, query) == NULL) {
        send_response(client_sock, "Failed to send message.\n");
        return;
    }

    // TODO: Gửi tin nhắn tới người nhận nếu họ đang online
    // Bạn cần triển khai cơ chế quản lý các kết nối đang online để tìm socket của người nhận
    // Ví dụ: bạn có thể sử dụng một danh sách các client đang kết nối và tìm socket dựa trên user_id

    send_response(client_sock, "Message sent successfully.\n");
}

void send_message_to_user(int recipient_sock, const char *message) {
    send(recipient_sock, message, strlen(message), 0);
}
