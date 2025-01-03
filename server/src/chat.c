// server/src/chat.c

#include "../include/chat.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BUFFER_SIZE 4048

void handle_private_message(DBConnection *db, int sender_id, const char *recipient_username, const char *message, int client_sock)
{
    // Tìm user_id của người nhận
    char query[BUFFER_SIZE];
    snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s'", recipient_username);
    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL)
    {
        send_response(client_sock, "Recipient not found.\n");
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL)
    {
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
    if (execute_query(db, query) == NULL)
    {
        send_response(client_sock, "Failed to send message.\n");
        return;
    }

    // TODO: Gửi tin nhắn tới người nhận nếu họ đang online
    // Bạn cần triển khai cơ chế quản lý các kết nối đang online để tìm socket của người nhận
    // Ví dụ: bạn có thể sử dụng một danh sách các client đang kết nối và tìm socket dựa trên user_id
}

void send_message_to_user(int recipient_sock, const char *message)
{
    send(recipient_sock, message, strlen(message), 0);
}

void handle_get_messages(DBConnection *conn, char *username, char *recipient, int client_socket)
{
    char query[BUFFER_SIZE];
    char chat_history[BUFFER_SIZE * 10] = "CHAT_HISTORY:";

    if (conn == NULL)
    {
        fprintf(stderr, "Database connection is NULL.\n");
        send_response(client_socket, "Database connection failed.\n");
        return;
    }

    // Tạo câu truy vấn dựa trên người nhận (group hoặc private)
    if (recipient[0] == '#')
    {
        snprintf(query, sizeof(query),
                 "SELECT u.username, m.message "
                 "FROM messages m "
                 "JOIN users u ON m.sender_id = u.id "
                 "WHERE m.group_id = (SELECT id FROM chat_groups WHERE group_name='%s') "
                 "ORDER BY m.created_at",
                 recipient + 1);
    }
    else
    {
        snprintf(query, sizeof(query),
                 "SELECT u.username, m.message "
                 "FROM messages m "
                 "JOIN users u ON m.sender_id = u.id "
                 "WHERE "
                 "(m.sender_id = (SELECT id FROM users WHERE username='%s') AND "
                 " m.receiver_id = (SELECT id FROM users WHERE username='%s')) OR "
                 "(m.sender_id = (SELECT id FROM users WHERE username='%s') AND "
                 " m.receiver_id = (SELECT id FROM users WHERE username='%s')) "
                 "ORDER BY m.created_at",
                 username, recipient, recipient, username);
    }

    printf("Executing query: %s\n", query);

    // Thực hiện truy vấn
    MYSQL_RES *res = execute_query(conn, query);
    if (res == NULL)
    {
        send_response(client_socket, "Failed to retrieve messages.\n");
        return;
    }

    // Đọc kết quả truy vấn
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        strcat(chat_history, row[0]); // Username
        strcat(chat_history, ": ");
        strcat(chat_history, row[1]); // Message
        strcat(chat_history, "\n");
    }

    if (strlen(chat_history) > strlen("CHAT_HISTORY:"))
    {
        write(client_socket, chat_history, strlen(chat_history));
    }
    else
    {
        write(client_socket, "CHAT_HISTORY:No messages found.\n", strlen("CHAT_HISTORY:No messages found.\n"));
    }

    // Giải phóng tài nguyên
    mysql_free_result(res);
}
