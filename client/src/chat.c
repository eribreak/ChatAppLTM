// client/src/chat.c

#include "../include/chat.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MESSAGE_BUFFER 1024
#define USERNAME_BUFFER 50

typedef struct {
    int socket;
    char username[USERNAME_BUFFER];
} chat_thread_data;

// Hàm nhận tin nhắn từ server
void *receive_messages(void *arg) {
    chat_thread_data *data = (chat_thread_data *)arg;
    int sock = data->socket;
    char buffer[MESSAGE_BUFFER];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            printf("Đã ngắt kết nối từ server.\n");
            break;
        }
        printf("\n%s", buffer);
        printf(">> ");
        fflush(stdout);
    }
    return NULL;
}

int init_chat(const char *server_ip, int port, const char *username) {
    int sock;
    struct sockaddr_in server_address;

    // Tạo socket TCP
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        handle_error("Không thể tạo socket");
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    // Chuyển đổi địa chỉ IP
    if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) <= 0) {
        handle_error("Địa chỉ IP không hợp lệ");
    }

    // Kết nối đến server
    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        handle_error("Kết nối đến server thất bại");
    }

    // Gửi username đến server
    send(sock, username, strlen(username), 0);

    printf("Đã kết nối đến server.\n");

    // Tạo thread để nhận tin nhắn
    pthread_t recv_thread;
    chat_thread_data data;
    data.socket = sock;
    strncpy(data.username, username, USERNAME_BUFFER - 1);
    pthread_create(&recv_thread, NULL, receive_messages, (void *)&data);

    // Trả về socket để tiếp tục gửi tin nhắn
    return sock;
}

void send_private_message(int sock, const char *recipient, const char *message) {
    char buffer[MESSAGE_BUFFER];
    snprintf(buffer, sizeof(buffer), "PRIVATE %s: %s", recipient, message);
    send(sock, buffer, strlen(buffer), 0);
}

void handle_chat(int sock, const char *username) {
    char input[MESSAGE_BUFFER];
    char recipient[USERNAME_BUFFER];
    char message[MESSAGE_BUFFER];

    printf("Nhập tin nhắn dưới định dạng: <username> <message>\n");
    printf("Ví dụ: Alice Hello!\n");

    while (1) {
        printf(">> ");
        fgets(input, sizeof(input), stdin);

        // Kiểm tra nếu người dùng muốn thoát
        if (strncmp(input, "Exit", 4) == 0) {
            printf("Đang thoát...\n");
            close(sock);
            exit(0);
        }

        // Tách recipient và message
        char *space = strchr(input, ' ');
        if (space == NULL) {
            printf("Định dạng không hợp lệ. Vui lòng thử lại.\n");
            continue;
        }

        size_t recipient_len = space - input;
        strncpy(recipient, input, recipient_len);
        recipient[recipient_len] = '\0';

        strncpy(message, space + 1, sizeof(message) - 1);
        message[strcspn(message, "\n")] = '\0'; // Loại bỏ ký tự newline

        send_private_message(sock, recipient, message);
    }
}
