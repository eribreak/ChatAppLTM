// server/src/main.c

#include "../include/db.h"
#include "../include/chat.h"
#include "../include/group.h"
#include "../include/file.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100
#define PORT 8083

Client *clients[MAX_CLIENTS] = {NULL};
DBConnection db;
int current_index;

// Hàm xử lý kết nối của từng client
void *handle_client(int client_index)
{
    Client *client = clients[client_index];
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytesReceived = recv(client->sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0)
    {
        printf("Client disconnected or error occurred.\n");
        close(client->sock);
        free(client);
    }
    buffer[bytesReceived] = '\0';

    printf("Received: %s\n", buffer);
    // Xử lý đăng ký hoặc đăng nhập
    if (strncmp(buffer, "REGISTER", 8) == 0)
    {
        char username[50], password[50], email[100];
        // Đảm bảo rằng bạn có đủ dữ liệu trước khi sử dụng sscanf
        int scanned = sscanf(buffer + 9, "%49s %49s %99s", username, password, email);
        if (scanned != 3)
        {
            send_response(client->sock, "RegisterFailed: Invalid data format.\n");
            close(client->sock);
            free(client);
        }

        // Kiểm tra xem người dùng đã tồn tại chưa
        char query[BUFFER_SIZE];
        snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s'", username);
        MYSQL_RES *res = execute_query(&db, query);
        if (res != NULL && mysql_num_rows(res) > 0)
        {
            send_response(client->sock, "UserExists\n");
            mysql_free_result(res);
        }
        else
        {
            if (res != NULL && res != (MYSQL_RES *)1)
            {
                mysql_free_result(res);
            }
            // Thêm người dùng vào cơ sở dữ liệu
            snprintf(query, sizeof(query),
                     "INSERT INTO users (username, password, email, status) VALUES ('%s', '%s', '%s', 'offline')",
                     username, password, email);
            MYSQL_RES *insert_res = execute_query(&db, query);
            if (insert_res == NULL)
            {
                send_response(client->sock, "RegisterFailed\n");
            }
            else
            {
                // Kiểm tra xem insert_res là (MYSQL_RES *)1 hay không
                if (insert_res == (MYSQL_RES *)1)
                {
                    // Lấy ID người dùng vừa đăng ký
                    int user_id = mysql_insert_id(db.conn);
                    client->id = user_id;
                    strncpy(client->username, username, sizeof(client->username) - 1);
                    send_response(client->sock, "RegisterSuccess\n");
                    printf("User %s đã đăng ký.\n", username);
                }
                else
                {
                    // Trường hợp này không nên xảy ra với INSERT
                    send_response(client->sock, "RegisterFailed\n");
                }
            }
        }
    }
    else if (strncmp(buffer, "LOGIN", 5) == 0)
    {
        char username[50], password[50];
        int scanned = sscanf(buffer + 6, "%49s %49s", username, password);
        if (scanned != 2)
        {
            send_response(client->sock, "LoginFailed: Invalid data format.\n");
            close(client->sock);
            free(client);
        }

        // Kiểm tra thông tin đăng nhập
        char query[BUFFER_SIZE];
        snprintf(query, sizeof(query),
                 "SELECT id FROM users WHERE username='%s' AND password='%s'",
                 username, password);
        MYSQL_RES *res = execute_query(&db, query);
        if (res != NULL && mysql_num_rows(res) > 0)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            int user_id = atoi(row[0]);
            client->id = user_id;
            strncpy(client->username, username, sizeof(client->username) - 1);
            send_response(client->sock, "LoginSuccess\n");

            // Cập nhật trạng thái online
            snprintf(query, sizeof(query), "UPDATE users SET status='online' WHERE id=%d", user_id);
            execute_query(&db, query);

            printf("User %s đã đăng nhập.\n", username);
            mysql_free_result(res);
        }
        else
        {
            send_response(client->sock, "LoginFailed\n");
            if (res != NULL)
            {
                mysql_free_result(res);
            }
            close(client->sock);
            free(client);
        }
    }
    else if (strncmp(buffer, "PRIVATE", 7) == 0)
    {
        char recipient[50];
        char message[BUFFER_SIZE];
        int scanned = sscanf(buffer + 8, "%49[^:]: %[^\n]", recipient, message);
        if (scanned < 2)
        {
            send_response(client->sock, "PrivateMessageFailed: Invalid format.\n");
        }
        printf("%s %s\n", recipient, message);
        handle_private_message(&db, client->id, recipient, message, client->sock);
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] != NULL && strcmp(recipient, clients[i]->username) == 0)
            {
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "MESSAGE: %s : %s", client->username, message);
                printf("%s\n", response);
                send_response(clients[i]->sock, response);
            }
        }
    }
    else if (strncmp(buffer, "CREATE_GROUP", 12) == 0)
    {
        char group_name[50];
        int scanned = sscanf(buffer + 13, "%49s", group_name);
        if (scanned != 1)
        {
            send_response(client->sock, "CreateGroupFailed: Invalid format.\n");
        }
        printf("%s\n", group_name);
        int group_id = create_group(&db, group_name, client->id);
        if (group_id == -1)
        {
            send_response(client->sock, "CreateGroupFailed\n");
        }
        else
        {
            printf("%d\n", group_id);
            send_response(client->sock, "GroupCreated\n");
        }
    }
    else if (strncmp(buffer, "JOIN_GROUP", 10) == 0)
    {
        char group_name[50];
        int scanned = sscanf(buffer + 11, "%49s", group_name);
        if (scanned != 1)
        {
            send_response(client->sock, "JoinGroupFailed: Invalid format.\n");
        }
        int group_id = join_group(&db, group_name, client->id);
        if (group_id == -1)
        {
            send_response(client->sock, "JoinGroupFailed\n");
        }
        else if (group_id == -2)
        {
            send_response(client->sock, "AlreadyMember\n");
        }
        else
        {
            send_response(client->sock, "JoinedGroup\n");
        }
    }
    else if (strncmp(buffer, "LEAVE_GROUP", 11) == 0)
    {
        char group_name[50];
        int scanned = sscanf(buffer + 12, "%49s", group_name);
        if (scanned != 1)
        {
            send_response(client->sock, "LeaveGroupFailed: Invalid format.\n");
        }
        if (leave_group(&db, group_name, client->id) == 0)
        {
            send_response(client->sock, "LeftGroup\n");
        }
        else
        {
            send_response(client->sock, "LeaveGroupFailed\n");
        }
    }
    else if (strncmp(buffer, "ADD_MEMBER", 10) == 0)
    {
        char group_name[50], member_username[50];
        int scanned = sscanf(buffer + 11, "%49s %49s", group_name, member_username);
        if (scanned != 2)
        {
            send_response(client->sock, "AddMemberFailed: Invalid format.\n");
        }
        int add_result = add_member_to_group(&db, group_name, member_username);
        if (add_result == 0)
        {
            send_response(client->sock, "AddMemberSuccess\n");
        }
        else if (add_result == -2)
        {
            send_response(client->sock, "MemberAlreadyExists\n");
        }
        else
        {
            send_response(client->sock, "AddMemberFailed\n");
        }
    }
    else if (strncmp(buffer, "REMOVE_MEMBER", 13) == 0)
    {
        char group_name[50], member_username[50];
        int scanned = sscanf(buffer + 14, "%49s %49s", group_name, member_username);
        if (scanned != 2)
        {
            send_response(client->sock, "RemoveMemberFailed: Invalid format.\n");
        }
        if (remove_member_from_group(&db, group_name, member_username) == 0)
        {
            send_response(client->sock, "RemoveMemberSuccess\n");
        }
        else
        {
            send_response(client->sock, "RemoveMemberFailed\n");
        }
    }
    else if (strncmp(buffer, "SEND", 4) == 0)
    {
        char group_name[50];
        char message[BUFFER_SIZE];
        int scanned = sscanf(buffer + 5, "%49s %[^\n]", group_name, message);
        if (scanned < 2)
        {
            send_response(client->sock, "GroupMessageFailed: Invalid format.\n");
        }
        if (send_group_message(&db, client, group_name, message, clients) != 0)
        {
            send_response(client->sock, "GroupMessageFailed\n");
        }
    }
    else if (strncmp(buffer, "HISTORY", 7) == 0)
    {
        char recipient[50];
        sscanf(buffer + 8, "%s", recipient);
        printf("%s\n", recipient);
        handle_get_messages(&db, client->username, recipient, client->sock);
    }
    else if (strncmp(buffer, "LIST_GROUPS", 11) == 0)
    {
        char groups[BUFFER_SIZE];
        if (list_user_groups(&db, client->id, groups, sizeof(groups)) == 0)
        {
            send(client->sock, groups, strlen(groups), 0);
        }
        else
        {
            send_response(client->sock, "ListGroupsFailed\n");
        }
    }
    else if (strncmp(buffer, "LIST_USERS", 10) == 0)
    {
        char users[BUFFER_SIZE];
        if (list_all_users(&db, users, sizeof(users)) == 0)
        {
            send(client->sock, users, strlen(users), 0);
        }
        else
        {
            send_response(client->sock, "ListUsersFailed\n");
        }
    }
    else if (strncmp(buffer, "SEARCH", 6) == 0)
    {
        char query_str[BUFFER_SIZE] = "";
        if (strlen(buffer) > 6)
        {
            int scanned = sscanf(buffer + 7, "%1023s", query_str);
            if (scanned != 1)
            {
                send_response(client->sock, "SearchFailed: Invalid format.\n");
            }
        }
        char search_results[BUFFER_SIZE];

        if (search_files(&db, query_str, client->id, search_results, sizeof(search_results)) == 0)
        {
            printf("%s\n", search_results);
            send_response(client->sock, search_results);
        }
        else
        {
            send_response(client->sock, "SearchFailed\n");
        }
    }
    else if (strncmp(buffer, "UPLOAD_PRIVATE", 14) == 0)
    {
        char receiver_username[50], file_path[BUFFER_SIZE], file_type[50], file_name[BUFFER_SIZE];
        unsigned long long filesize;
        char *file_content = NULL; // Khởi tạo con trỏ file_content là NULL

        // Tìm vị trí của ký tự '\n' để phân tách phần lệnh và nội dung file
        char *newline_pos = strchr(buffer, '\n');
        if (newline_pos == NULL)
        {
            send_response(client->sock, "UploadPrivateFailed: Invalid format.\n");
            return NULL;
        }

        // Tách phần lệnh và nội dung file
        *newline_pos = '\0';            // Kết thúc phần lệnh
        file_content = newline_pos + 1; // Phần còn lại là nội dung file

        // Kiểm tra nếu file_content có dữ liệu hợp lệ
        if (file_content == NULL || strlen(file_content) == 0)
        {
            send_response(client->sock, "UploadPrivateFailed: Missing file content.\n");
            return NULL;
        }

        // Phân tích phần lệnh để lấy receiver_username và file_path
        int scanned = sscanf(buffer + 15, "%49s %llu %1023s", receiver_username, &filesize, file_path); // Chỉ đọc username và file_path
        if (scanned != 3)
        {
            send_response(client->sock, "UploadPrivateFailed: Invalid format.\n");
            return NULL;
        }

        // Lấy file_type từ đường dẫn
        const char *ext = strrchr(file_path, '.'); // Tìm phần mở rộng file
        if (ext == NULL || strlen(ext) < 2)
        {
            send_response(client->sock, "UploadPrivateFailed: Invalid file type.\n");
            return NULL;
        }
        else
        {
            strncpy(file_type, ext + 1, sizeof(file_type) - 1); // Bỏ dấu '.' ở đầu phần mở rộng
            file_type[sizeof(file_type) - 1] = '\0';
        }

        // Lấy file_name từ file_path
        const char *slash = strrchr(file_path, '/'); // Tìm phần cuối cùng sau dấu '/'
        if (slash == NULL)
        {
            strncpy(file_name, file_path, sizeof(file_name) - 1); // Nếu không có '/', file_path chính là file_name
        }
        else
        {
            strncpy(file_name, slash + 1, sizeof(file_name) - 1); // Bỏ dấu '/' ở đầu
        }
        file_name[sizeof(file_name) - 1] = '\0';
        printf("%s\n", file_path);
        // Giả sử bạn lưu file vào thư mục và cần sử dụng file size để kiểm tra khi lưu
        if (upload_file(&db, client->id, receiver_username, file_name, file_type, file_content, filesize, 0) == 0)
        { // is_group = 0
            send_response(client->sock, "UploadPrivateSuccess");
        }
        else
        {
            send_response(client->sock, "UploadPrivateFail");
        }
    }

    else if (strncmp(buffer, "UPLOAD_GROUP", 12) == 0)
    {
        char receiver_username[50], file_path[BUFFER_SIZE], file_type[50], file_name[BUFFER_SIZE];
        unsigned long long filesize;
        char *file_content = NULL; // Khởi tạo con trỏ file_content là NULL

        // Tìm vị trí của ký tự '\n' để phân tách phần lệnh và nội dung file
        char *newline_pos = strchr(buffer, '\n');
        if (newline_pos == NULL)
        {
            send_response(client->sock, "UploadGroupFailed: Invalid format.\n");
            return NULL;
        }

        // Tách phần lệnh và nội dung file
        *newline_pos = '\0';            // Kết thúc phần lệnh
        file_content = newline_pos + 1; // Phần còn lại là nội dung file

        // Kiểm tra nếu file_content có dữ liệu hợp lệ
        if (file_content == NULL || strlen(file_content) == 0)
        {
            send_response(client->sock, "UploadGroupFailed: Missing file content.\n");
            return NULL;
        }

        // Phân tích phần lệnh để lấy receiver_username và file_path
        int scanned = sscanf(buffer + 13, "%49s %llu %1023s", receiver_username, &filesize, file_path); // Chỉ đọc username và file_path
        if (scanned != 3)
        {
            send_response(client->sock, "UploadGroupFailed: Invalid format.\n");
            return NULL;
        }

        // Lấy file_type từ đường dẫn
        const char *ext = strrchr(file_path, '.'); // Tìm phần mở rộng file
        if (ext == NULL || strlen(ext) < 2)
        {
            send_response(client->sock, "UploadGroupFailed: Invalid file type.\n");
            return NULL;
        }
        else
        {
            strncpy(file_type, ext + 1, sizeof(file_type) - 1); // Bỏ dấu '.' ở đầu phần mở rộng
            file_type[sizeof(file_type) - 1] = '\0';
        }

        // Lấy file_name từ file_path
        const char *slash = strrchr(file_path, '/'); // Tìm phần cuối cùng sau dấu '/'
        if (slash == NULL)
        {
            strncpy(file_name, file_path, sizeof(file_name) - 1); // Nếu không có '/', file_path chính là file_name
        }
        else
        {
            strncpy(file_name, slash + 1, sizeof(file_name) - 1); // Bỏ dấu '/' ở đầu
        }
        file_name[sizeof(file_name) - 1] = '\0';
        printf("%s\n", file_path);
        // Giả sử bạn lưu file vào thư mục và cần sử dụng file size để kiểm tra khi lưu
        if (upload_file(&db, client->id, receiver_username, file_name, file_type, file_content, filesize, 1) == 0)
        { // is_group = 0
            send_response(client->sock, "UploadGroupSuccess");
        }
        else
        {
            send_response(client->sock, "UploadGroupFail");
        }
    }
    else if (strncmp(buffer, "DOWNLOAD", 8) == 0)
    {
        char file_name[BUFFER_SIZE];
        char file_content[BUFFER_SIZE];
        char response[BUFFER_SIZE];
        int scanned = sscanf(buffer + 9, "%1023s", file_name);
        if (scanned != 1)
        {
            send_response(client->sock, "DownloadFailed: Invalid format.\n");
        }
        else
        {
            if (download_file(&db, client->id, file_name, file_content) == 0)
            {
                snprintf(response, sizeof(response), "DownloadSuccess %s", file_content);
                printf("%s\n", response);
                send_response(client->sock, response);
            }
            else
            {
                send_response(client->sock, "DownloadFail");
            }
        }
    }
    return NULL;
}

int main()
{
    if (connect_db(&db, "localhost", "root", "", "chat_app") != EXIT_SUCCESS)
    {
        fprintf(stderr, "Failed to connect to database.\n");
        exit(EXIT_FAILURE);
    }

    int server_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    fd_set readfds;
    int max_sd;

    // Tạo socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 10) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server đang lắng nghe trên cổng %d...\n", PORT);

    // Khởi tạo danh sách client
    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i] = NULL;

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(server_sock, &readfds);
        max_sd = server_sock;

        // Thêm các socket của client vào `readfds`
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] != NULL && clients[i]->sock > 0)
            {
                FD_SET(clients[i]->sock, &readfds);
                if (clients[i]->sock > max_sd)
                    max_sd = clients[i]->sock;
            }
        }

        struct timespec timeout = {5, 0}; // 5 giây
        int activity = pselect(max_sd + 1, &readfds, NULL, NULL, &timeout, NULL);

        if (activity < 0 && errno != EINTR)
        {
            perror("pselect error");
            continue;
        }

        // Kiểm tra kết nối mới
        if (FD_ISSET(server_sock, &readfds))
        {
            int new_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
            if (new_sock < 0)
            {
                perror("Accept failed");
                continue;
            }

            printf("Client mới kết nối.\n");

            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i] == NULL)
                {
                    clients[i] = (Client *)malloc(sizeof(Client));
                    clients[i]->sock = new_sock;
                    clients[i]->id = -1;
                    memset(clients[i]->username, 0, sizeof(clients[i]->username));
                    break;
                }
            }
        }

        // Xử lý dữ liệu từ các client
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] != NULL && FD_ISSET(clients[i]->sock, &readfds))
            {
                handle_client(i);
            }
        }
    }

    close(server_sock);
    close_db(&db);
    return 0;
}