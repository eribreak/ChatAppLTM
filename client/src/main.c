// client/src/main.c

#include "../include/chat.h"
#include "../include/group.h"
#include "../include/file.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
#define MESSAGE_BUFFER 1024
#define USERNAME_BUFFER 254
#define PORT 8083

int main()
{
    char username[50];
    char password[50];
    char email[100];
    int choice;

    printf("==== Đăng Ký / Đăng Nhập ====\n");
    printf("1. Đăng ký\n2. Đăng nhập\nLựa chọn: ");
    if (scanf("%d", &choice) != 1)
    {
        printf("Lựa chọn không hợp lệ.\n");
        exit(EXIT_FAILURE);
    }
    getchar(); // Xóa ký tự newline

    // Kết nối đến server để đăng ký hoặc đăng nhập
    int sock;
    struct sockaddr_in server_address;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        handle_error("Không thể tạo socket");
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) <= 0)
    {
        handle_error("Địa chỉ IP không hợp lệ");
    }

    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        handle_error("Kết nối đến server thất bại");
    }

    if (choice == 1)
    {
        // Đăng ký
        printf("Nhập tên người dùng: ");
        if (fgets(username, sizeof(username), stdin) == NULL)
        {
            printf("Đọc tên người dùng thất bại.\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
        username[strcspn(username, "\n")] = 0;

        printf("Nhập mật khẩu: ");
        if (fgets(password, sizeof(password), stdin) == NULL)
        {
            printf("Đọc mật khẩu thất bại.\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
        password[strcspn(password, "\n")] = 0;

        printf("Nhập email: ");
        if (fgets(email, sizeof(email), stdin) == NULL)
        {
            printf("Đọc email thất bại.\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
        email[strcspn(email, "\n")] = 0;

        // Gửi yêu cầu đăng ký đến server
        char register_buffer[BUFFER_SIZE];
        snprintf(register_buffer, sizeof(register_buffer), "REGISTER %s %s %s", username, password, email);
        if (send(sock, register_buffer, strlen(register_buffer), 0) < 0)
        {
            handle_error("Gửi yêu cầu đăng ký thất bại");
        }

        // Nhận phản hồi từ server
        char response[BUFFER_SIZE];
        ssize_t bytesReceived = recv(sock, response, sizeof(response) - 1, 0);
        if (bytesReceived > 0)
        {
            response[bytesReceived] = '\0';
            // Loại bỏ ký tự dòng mới nếu có
            response[strcspn(response, "\n")] = 0;
            printf("Server: %s\n", response);
            if (strcmp(response, "RegisterSuccess") == 0)
            {
                printf("Đăng ký thành công! Bạn có thể đăng nhập ngay.\n");
            }
            else
            {
                printf("Đăng ký thất bại: %s\n", response);
                close(sock);
                exit(0);
            }
        }
        else
        {
            printf("Không nhận được phản hồi từ server.\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
    }
    else if (choice == 2)
    {
        // Đăng nhập
        printf("Nhập tên người dùng: ");
        if (fgets(username, sizeof(username), stdin) == NULL)
        {
            printf("Đọc tên người dùng thất bại.\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
        username[strcspn(username, "\n")] = 0;

        printf("Nhập mật khẩu: ");
        if (fgets(password, sizeof(password), stdin) == NULL)
        {
            printf("Đọc mật khẩu thất bại.\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
        password[strcspn(password, "\n")] = 0;

        // Gửi yêu cầu đăng nhập đến server
        char login_buffer[BUFFER_SIZE];
        snprintf(login_buffer, sizeof(login_buffer), "LOGIN %s %s", username, password);
        if (send(sock, login_buffer, strlen(login_buffer), 0) < 0)
        {
            handle_error("Gửi yêu cầu đăng nhập thất bại");
        }

        // Nhận phản hồi từ server
        char response[BUFFER_SIZE];
        ssize_t bytesReceived = recv(sock, response, sizeof(response) - 1, 0);
        if (bytesReceived > 0)
        {
            response[bytesReceived] = '\0';
            // Loại bỏ ký tự dòng mới nếu có
            response[strcspn(response, "\n")] = 0;
            printf("Server: %s\n", response);
            if (strcmp(response, "LoginSuccess") == 0)
            {
                printf("Đăng nhập thành công!\n");
            }
            else
            {
                printf("Đăng nhập thất bại: %s\n", response);
                close(sock);
                exit(0);
            }
        }
        else
        {
            printf("Không nhận được phản hồi từ server.\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        printf("Lựa chọn không hợp lệ.\n");
        close(sock);
        exit(0);
    }
    int main_choice;
    while (1)
    {
        printf("\n==== Menu Chính ====\n");
        printf("1. Chat 1-1\n");
        printf("2. Chat Nhóm\n");
        printf("3. Chia Sẻ File\n");
        printf("4. Tìm Kiếm File\n");
        printf("5. Thoát\n");
        printf("Lựa chọn: ");
        if (scanf("%d", &main_choice) != 1)
        {
            printf("Lựa chọn không hợp lệ.\n");
            while (getchar() != '\n')
                ; // Xóa bộ đệm nhập
            continue;
        }
        getchar(); // Xóa ký tự newline

        if (main_choice == 1)
        {
            // Chat 1-1
            char recipient[USERNAME_BUFFER];
            char message[MESSAGE_BUFFER];
            printf("Nhập tên người dùng muốn nhắn tin: ");
            if (fgets(recipient, sizeof(recipient), stdin) == NULL)
            {
                printf("Đọc tên người nhận thất bại.\n");
                continue;
            }
            recipient[strcspn(recipient, "\n")] = 0;

            printf("Nhập tin nhắn: ");
            if (fgets(message, sizeof(message), stdin) == NULL)
            {
                printf("Đọc tin nhắn thất bại.\n");
                continue;
            }
            message[strcspn(message, "\n")] = 0;

            send_private_message(sock, recipient, message);
        }
        else if (main_choice == 2)
        {
            // Chat Nhóm
            int group_choice;
            printf("\n==== Chat Nhóm ====\n");
            printf("1. Tạo nhóm\n");
            printf("2. Tham gia nhóm\n");
            printf("3. Rời nhóm\n");
            printf("4. Gửi tin nhắn nhóm\n");
            printf("5. Thêm thành viên vào nhóm\n");
            printf("6. Xóa thành viên khỏi nhóm\n");
            printf("7. Liệt kê các nhóm\n");
            printf("8. Liệt kê người dùng\n");
            printf("9. Quay lại Menu Chính\n");
            printf("Lựa chọn: ");
            if (scanf("%d", &group_choice) != 1)
            {
                printf("Lựa chọn không hợp lệ.\n");
                while (getchar() != '\n')
                    ; // Xóa bộ đệm nhập
                continue;
            }
            getchar(); // Xóa ký tự newline

            if (group_choice == 1)
            {
                char group_name[BUFFER_SIZE];
                printf("Nhập tên nhóm: ");
                if (fgets(group_name, sizeof(group_name), stdin) == NULL)
                {
                    printf("Đọc tên nhóm thất bại.\n");
                    continue;
                }
                group_name[strcspn(group_name, "\n")] = 0;
                create_group(sock, group_name);
            }
            else if (group_choice == 2)
            {
                char group_name[BUFFER_SIZE];
                printf("Nhập tên nhóm để tham gia: ");
                if (fgets(group_name, sizeof(group_name), stdin) == NULL)
                {
                    printf("Đọc tên nhóm thất bại.\n");
                    continue;
                }
                group_name[strcspn(group_name, "\n")] = 0;
                join_group(sock, group_name);
            }
            else if (group_choice == 3)
            {
                char group_name[BUFFER_SIZE];
                printf("Nhập tên nhóm để rời: ");
                if (fgets(group_name, sizeof(group_name), stdin) == NULL)
                {
                    printf("Đọc tên nhóm thất bại.\n");
                    continue;
                }
                group_name[strcspn(group_name, "\n")] = 0;
                leave_group(sock, group_name);
            }
            else if (group_choice == 4)
            {
                char group_name[BUFFER_SIZE];
                char message[MESSAGE_BUFFER];
                printf("Nhập tên nhóm: ");
                if (fgets(group_name, sizeof(group_name), stdin) == NULL)
                {
                    printf("Đọc tên nhóm thất bại.\n");
                    continue;
                }
                group_name[strcspn(group_name, "\n")] = 0;

                printf("Nhập tin nhắn: ");
                if (fgets(message, sizeof(message), stdin) == NULL)
                {
                    printf("Đọc tin nhắn thất bại.\n");
                    continue;
                }
                message[strcspn(message, "\n")] = 0;

                send_group_message(sock, group_name, message);
            }
            else if (group_choice == 5)
            {
                char group_name[BUFFER_SIZE];
                char member_username[USERNAME_BUFFER];
                printf("Nhập tên nhóm: ");
                if (fgets(group_name, sizeof(group_name), stdin) == NULL)
                {
                    printf("Đọc tên nhóm thất bại.\n");
                    continue;
                }
                group_name[strcspn(group_name, "\n")] = 0;

                printf("Nhập tên người dùng cần thêm: ");
                if (fgets(member_username, sizeof(member_username), stdin) == NULL)
                {
                    printf("Đọc tên người dùng thất bại.\n");
                    continue;
                }
                member_username[strcspn(member_username, "\n")] = 0;

                add_member(sock, group_name, member_username);
            }
            else if (group_choice == 6)
            {
                char group_name[BUFFER_SIZE];
                char member_username[USERNAME_BUFFER];
                printf("Nhập tên nhóm: ");
                if (fgets(group_name, sizeof(group_name), stdin) == NULL)
                {
                    printf("Đọc tên nhóm thất bại.\n");
                    continue;
                }
                group_name[strcspn(group_name, "\n")] = 0;

                printf("Nhập tên người dùng cần xóa: ");
                if (fgets(member_username, sizeof(member_username), stdin) == NULL)
                {
                    printf("Đọc tên người dùng thất bại.\n");
                    continue;
                }
                member_username[strcspn(member_username, "\n")] = 0;

                remove_member(sock, group_name, member_username);
            }
            else if (group_choice == 7)
            {
                list_groups(sock);
            }
            else if (group_choice == 8)
            {
                list_users(sock);
            }
            else if (group_choice == 9)
            {
                continue;
            }
            else
            {
                printf("Lựa chọn không hợp lệ.\n");
            }
        }
        else if (main_choice == 3)
        {
            // Chia Sẻ File
            int file_choice;
            printf("\n==== Chia Sẻ File ====\n");
            printf("1. Tải lên file\n");
            printf("2. Tải xuống file\n");
            printf("3. Tải lên thư mục\n");
            printf("4. Tải xuống thư mục\n");
            printf("5. Quay lại Menu Chính\n");
            printf("Lựa chọn: ");
            if (scanf("%d", &file_choice) != 1)
            {
                printf("Lựa chọn không hợp lệ.\n");
                while (getchar() != '\n')
                    ; // Xóa bộ đệm nhập
                continue;
            }
            getchar(); // Xóa ký tự newline

            if (file_choice == 1)
            {
                char file_path[BUFFER_SIZE];
                char receiver[USERNAME_BUFFER];
                int is_group;
                printf("1. Gửi tới người dùng\n2. Gửi tới nhóm\nLựa chọn: ");
                if (scanf("%d", &is_group) != 1)
                {
                    printf("Lựa chọn không hợp lệ.\n");
                    while (getchar() != '\n')
                        ; // Xóa bộ đệm nhập
                    continue;
                }
                getchar();

                printf("Nhập tên người dùng hoặc tên nhóm: ");
                if (fgets(receiver, sizeof(receiver), stdin) == NULL)
                {
                    printf("Đọc tên người nhận thất bại.\n");
                    continue;
                }
                receiver[strcspn(receiver, "\n")] = 0;

                printf("Nhập đường dẫn file: ");
                if (fgets(file_path, sizeof(file_path), stdin) == NULL)
                {
                    printf("Đọc đường dẫn file thất bại.\n");
                    continue;
                }
                file_path[strcspn(file_path, "\n")] = 0;

                upload_file(sock, file_path, receiver, is_group == 2);
            }
            else if (file_choice == 2)
            {
                char file_name[BUFFER_SIZE];
                char save_path[BUFFER_SIZE];
                printf("Nhập tên file cần tải xuống: ");
                if (fgets(file_name, sizeof(file_name), stdin) == NULL)
                {
                    printf("Đọc tên file thất bại.\n");
                    continue;
                }
                file_name[strcspn(file_name, "\n")] = 0;

                printf("Nhập đường dẫn để lưu file: ");
                if (fgets(save_path, sizeof(save_path), stdin) == NULL)
                {
                    printf("Đọc đường dẫn lưu file thất bại.\n");
                    continue;
                }
                save_path[strcspn(save_path, "\n")] = 0;

                download_file(sock, file_name, save_path);
            }
            else if (file_choice == 3)
            {
                char dir_path[BUFFER_SIZE];
                char receiver[USERNAME_BUFFER];
                int is_group;
                printf("1. Gửi tới người dùng\n2. Gửi tới nhóm\nLựa chọn: ");
                if (scanf("%d", &is_group) != 1)
                {
                    printf("Lựa chọn không hợp lệ.\n");
                    while (getchar() != '\n')
                        ; // Xóa bộ đệm nhập
                    continue;
                }
                getchar();

                printf("Nhập tên người dùng hoặc tên nhóm: ");
                if (fgets(receiver, sizeof(receiver), stdin) == NULL)
                {
                    printf("Đọc tên người nhận thất bại.\n");
                    continue;
                }
                receiver[strcspn(receiver, "\n")] = 0;

                printf("Nhập đường dẫn thư mục: ");
                if (fgets(dir_path, sizeof(dir_path), stdin) == NULL)
                {
                    printf("Đọc đường dẫn thư mục thất bại.\n");
                    continue;
                }
                dir_path[strcspn(dir_path, "\n")] = 0;

                upload_directory(sock, dir_path, receiver, is_group == 2);
            }
            else if (file_choice == 4)
            {
                char dir_name[BUFFER_SIZE];
                char save_path[BUFFER_SIZE];
                printf("Nhập tên thư mục cần tải xuống: ");
                if (fgets(dir_name, sizeof(dir_name), stdin) == NULL)
                {
                    printf("Đọc tên thư mục thất bại.\n");
                    continue;
                }
                dir_name[strcspn(dir_name, "\n")] = 0;

                printf("Nhập đường dẫn để lưu thư mục: ");
                if (fgets(save_path, sizeof(save_path), stdin) == NULL)
                {
                    printf("Đọc đường dẫn lưu thư mục thất bại.\n");
                    continue;
                }
                save_path[strcspn(save_path, "\n")] = 0;

                download_directory(sock, dir_name, save_path);
            }
            else if (file_choice == 5)
            {
                continue;
            }
            else
            {
                printf("Lựa chọn không hợp lệ.\n");
            }
        }
        else if (main_choice == 4)
        {
            // Tìm Kiếm File
            char query[BUFFER_SIZE];
            printf("Nhập từ khóa tìm kiếm: ");
            if (fgets(query, sizeof(query), stdin) == NULL)
            {
                printf("Đọc từ khóa tìm kiếm thất bại.\n");
                continue;
            }
            query[strcspn(query, "\n")] = 0;

            search_files(sock, query);
        }
        else if (main_choice == 5)
        {
            printf("Đang thoát...\n");
            close(sock);
            exit(0);
        }
        else
        {
            printf("Lựa chọn không hợp lệ.\n");
        }
        // Nhận phản hồi từ server
        char response[BUFFER_SIZE];
        ssize_t bytesReceived = recv(sock, response, sizeof(response) - 1, 0);

        if (bytesReceived > 0)
        {
            response[bytesReceived] = '\0';
            // Loại bỏ ký tự dòng mới nếu có
            response[strcspn(response, "\n")] = 0;
            printf("Server: %s\n", response);
        }
        else
        {
            printf("Không nhận được phản hồi từ server.\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
    }

    close(sock);
    return 0;
}
