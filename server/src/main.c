// server/src/main.c

#include "../include/db.h"
#include "../include/chat.h"
#include "../include/group.h"
#include "../include/file.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define CHAT_PORT 8085
#define GROUP_PORT 8086
#define FILE_PORT 8087

#define MAIN_PORT 8080

    typedef struct {
    int sock;
    int id;
    char username[50];
} Client;

DBConnection db;

// Hàm xử lý kết nối của từng client
void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    // Nhận yêu cầu đăng ký hoặc đăng nhập
    ssize_t bytesReceived = recv(client->sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
        close(client->sock);
        free(client);
        pthread_exit(NULL);
    }
    buffer[bytesReceived] = '\0';

    // Xử lý đăng ký hoặc đăng nhập
    if (strncmp(buffer, "REGISTER", 8) == 0) {
        char username[50], password[50], email[100];
        // Đảm bảo rằng bạn có đủ dữ liệu trước khi sử dụng sscanf
        int scanned = sscanf(buffer + 9, "%49s %49s %99s", username, password, email);
        if (scanned != 3) {
            send_response(client->sock, "RegisterFailed: Invalid data format.\n");
            close(client->sock);
            free(client);
            pthread_exit(NULL);
        }

        // Kiểm tra xem người dùng đã tồn tại chưa
        char query[BUFFER_SIZE];
        snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s'", username);
        MYSQL_RES *res = execute_query(&db, query);
        if (res != NULL && mysql_num_rows(res) > 0) {
            send_response(client->sock, "UserExists\n");
            mysql_free_result(res);
        } else {
            if (res != NULL && res != (MYSQL_RES *)1) {
                mysql_free_result(res);
            }
            // Thêm người dùng vào cơ sở dữ liệu
            snprintf(query, sizeof(query), 
                     "INSERT INTO users (username, password, email, status) VALUES ('%s', '%s', '%s', 'offline')", 
                     username, password, email);
            MYSQL_RES *insert_res = execute_query(&db, query);
            if (insert_res == NULL) {
                send_response(client->sock, "RegisterFailed\n");
            } else {
                // Kiểm tra xem insert_res là (MYSQL_RES *)1 hay không
                if (insert_res == (MYSQL_RES *)1) {
                    // Lấy ID người dùng vừa đăng ký
                    int user_id = mysql_insert_id(db.conn);
                    client->id = user_id;
                    strncpy(client->username, username, sizeof(client->username) - 1);
                    send_response(client->sock, "RegisterSuccess\n");
                    printf("User %s đã đăng ký.\n", username);
                } else {
                    // Trường hợp này không nên xảy ra với INSERT
                    send_response(client->sock, "RegisterFailed\n");
                }
            }
        }
    } 
    else if (strncmp(buffer, "LOGIN", 5) == 0) {
        char username[50], password[50];
        int scanned = sscanf(buffer + 6, "%49s %49s", username, password);
        if (scanned != 2) {
            send_response(client->sock, "LoginFailed: Invalid data format.\n");
            close(client->sock);
            free(client);
            pthread_exit(NULL);
        }

        // Kiểm tra thông tin đăng nhập
        char query[BUFFER_SIZE];
        snprintf(query, sizeof(query), 
                 "SELECT id FROM users WHERE username='%s' AND password='%s'", 
                 username, password);
        MYSQL_RES *res = execute_query(&db, query);
        if (res != NULL && mysql_num_rows(res) > 0) {
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
        } else {
            send_response(client->sock, "LoginFailed\n");
            if (res != NULL) {
                mysql_free_result(res);
            }
            close(client->sock);
            free(client);
            pthread_exit(NULL);
        }
    } 
    else {
        send_response(client->sock, "InvalidCommand\n");
        close(client->sock);
        free(client);
        pthread_exit(NULL);
    }

    // Vòng lặp xử lý các lệnh sau khi đăng nhập hoặc đăng ký thành công
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        bytesReceived = recv(client->sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            // Cập nhật trạng thái offline
            char update_query[BUFFER_SIZE];
            snprintf(update_query, sizeof(update_query), 
                     "UPDATE users SET status='offline' WHERE id=%d", client->id);
            execute_query(&db, update_query);

            printf("User %s đã ngắt kết nối.\n", client->username);
            close(client->sock);
            free(client);
            pthread_exit(NULL);
        }
        buffer[bytesReceived] = '\0';

        // Xử lý các lệnh
        if (strncmp(buffer, "PRIVATE", 7) == 0) {
            char recipient[50];
            char message[BUFFER_SIZE];
            int scanned = sscanf(buffer + 8, "%49s %[^\n]", recipient, message);
            if (scanned < 2) {
                send_response(client->sock, "PrivateMessageFailed: Invalid format.\n");
                continue;
            }
            handle_private_message(&db, client->id, recipient, message, client->sock);
        } 
        else if (strncmp(buffer, "CREATE_GROUP", 12) == 0) {
            char group_name[50];
            int scanned = sscanf(buffer + 13, "%49s", group_name);
            if (scanned != 1) {
                send_response(client->sock, "CreateGroupFailed: Invalid format.\n");
                continue;
            }
            int group_id = create_group(&db, group_name, client->id);
            if (group_id == -1) {
                send_response(client->sock, "CreateGroupFailed\n");
            } else {
                send_response(client->sock, "GroupCreated\n");
            }
        } 
        else if (strncmp(buffer, "JOIN_GROUP", 10) == 0) {
            char group_name[50];
            int scanned = sscanf(buffer + 11, "%49s", group_name);
            if (scanned != 1) {
                send_response(client->sock, "JoinGroupFailed: Invalid format.\n");
                continue;
            }
            int group_id = join_group(&db, group_name, client->id);
            if (group_id == -1) {
                send_response(client->sock, "JoinGroupFailed\n");
            } else if (group_id == -2) {
                send_response(client->sock, "AlreadyMember\n");
            } else {
                send_response(client->sock, "JoinedGroup\n");
            }
        } 
        else if (strncmp(buffer, "LEAVE_GROUP", 11) == 0) {
            char group_name[50];
            int scanned = sscanf(buffer + 12, "%49s", group_name);
            if (scanned != 1) {
                send_response(client->sock, "LeaveGroupFailed: Invalid format.\n");
                continue;
            }
            if (leave_group(&db, group_name, client->id) == 0) {
                send_response(client->sock, "LeftGroup\n");
            } else {
                send_response(client->sock, "LeaveGroupFailed\n");
            }
        } 
        else if (strncmp(buffer, "ADD_MEMBER", 10) == 0) {
            char group_name[50], member_username[50];
            int scanned = sscanf(buffer + 11, "%49s %49s", group_name, member_username);
            if (scanned != 2) {
                send_response(client->sock, "AddMemberFailed: Invalid format.\n");
                continue;
            }
            int add_result = add_member_to_group(&db, group_name, member_username);
            if (add_result == 0) {
                send_response(client->sock, "AddMemberSuccess\n");
            } else if (add_result == -2) {
                send_response(client->sock, "MemberAlreadyExists\n");
            } else {
                send_response(client->sock, "AddMemberFailed\n");
            }
        } 
        else if (strncmp(buffer, "REMOVE_MEMBER", 13) == 0) {
            char group_name[50], member_username[50];
            int scanned = sscanf(buffer + 14, "%49s %49s", group_name, member_username);
            if (scanned != 2) {
                send_response(client->sock, "RemoveMemberFailed: Invalid format.\n");
                continue;
            }
            if (remove_member_from_group(&db, group_name, member_username) == 0) {
                send_response(client->sock, "RemoveMemberSuccess\n");
            } else {
                send_response(client->sock, "RemoveMemberFailed\n");
            }
        } 
        else if (strncmp(buffer, "SEND", 4) == 0) {
            char group_name[50];
            char message[BUFFER_SIZE];
            int scanned = sscanf(buffer + 5, "%49s %[^\n]", group_name, message);
            if (scanned < 2) {
                send_response(client->sock, "GroupMessageFailed: Invalid format.\n");
                continue;
            }
            if (send_group_message(&db, client->id, group_name, message) == 0) {
                send_response(client->sock, "GroupMessageSent\n");
            } else {
                send_response(client->sock, "GroupMessageFailed\n");
            }
        } 
        else if (strncmp(buffer, "LIST_GROUPS", 11) == 0) {
            char groups[BUFFER_SIZE];
            if (list_user_groups(&db, client->id, groups, sizeof(groups)) == 0) {
                send(client->sock, groups, strlen(groups), 0);
            } else {
                send_response(client->sock, "ListGroupsFailed\n");
            }
        } 
        else if (strncmp(buffer, "LIST_USERS", 10) == 0) {
            char users[BUFFER_SIZE];
            if (list_all_users(&db, users, sizeof(users)) == 0) {
                send(client->sock, users, strlen(users), 0);
            } else {
                send_response(client->sock, "ListUsersFailed\n");
            }
        } 
        else if (strncmp(buffer, "SEARCH", 6) == 0) {
            char query_str[BUFFER_SIZE];
            int scanned = sscanf(buffer + 7, "%1023s", query_str);
            if (scanned != 1) {
                send_response(client->sock, "SearchFailed: Invalid format.\n");
                continue;
            }
            char search_results[BUFFER_SIZE];
            if (search_files(&db, query_str, search_results, sizeof(search_results)) == 0) {
                send(client->sock, search_results, strlen(search_results), 0);
                send_response(client->sock, "END_OF_RESULTS\n");
            } else {
                send_response(client->sock, "SearchFailed\n");
            }
        } 
        else {
            send_response(client->sock, "UnknownCommand\n");
        }
    }

    close(client->sock);
    free(client);
    pthread_exit(NULL);
}

int main() {
    // Kết nối đến cơ sở dữ liệu
    if (connect_db(&db, "localhost", "root", "transyhieu123", "chat_app") != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to connect to database.\n");
        exit(EXIT_FAILURE);
    }

    int server_sock, new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Tạo socket TCP cho chat
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        handle_error("Socket failed");
    }

    // Thiết lập địa chỉ
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(CHAT_PORT);

    // Gắn socket với địa chỉ
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        handle_error("Bind failed");
    }

    // Lắng nghe kết nối
    if (listen(server_sock, 10) < 0) {
        handle_error("Listen failed");
    }

    printf("Server đang lắng nghe trên cổng %d...\n", CHAT_PORT);

    while (1) {
        if ((new_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        // Tạo client mới
        Client *client = (Client *)malloc(sizeof(Client));
        if (client == NULL) {
            perror("Malloc failed");
            close(new_sock);
            continue;
        }
        client->sock = new_sock;
        client->id = -1;
        memset(client->username, 0, sizeof(client->username));

        // Tạo thread để xử lý client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)client) != 0) {
            perror("pthread_create failed");
            close(new_sock);
            free(client);
            continue;
        }
        pthread_detach(thread_id);
    }

    close(server_sock);
    close_db(&db);
    return 0;
}


// server/src/main.c

#include "../include/db.h"
#include "../include/chat.h"
#include "../include/group.h"
#include "../include/file.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define MAIN_PORT 8085

typedef struct {
    int sock;
    int id;
    char username[50];
} Client;

DBConnection db;

// Hàm xử lý các lệnh từ client
int handle_commands(DBConnection *db, int client_sock, int user_id, const char *username, char *buffer) {
    char *command = strtok(buffer, " ");
    if (command == NULL) {
        send_response(client_sock, "InvalidCommand\n");
        return -1;
    }

    if (strcmp(command, "CREATE_GROUP") == 0) {
        // Xử lý tạo nhóm
        // ... (như trong mã nguồn bạn đã cung cấp)
    }
    else if (strcmp(command, "JOIN_GROUP") == 0) {
        // Xử lý tham gia nhóm
        // ... (như trong mã nguồn bạn đã cung cấp)
    }
    else if (strcmp(command, "LEAVE_GROUP") == 0) {
        // Xử lý rời nhóm
        // ... (như trong mã nguồn bạn đã cung cấp)
    }
    else if (strcmp(command, "ADD_MEMBER") == 0) {
        // Xử lý thêm thành viên vào nhóm
        // ... (như trong mã nguồn bạn đã cung cấp)
    }
    else if (strcmp(command, "REMOVE_MEMBER") == 0) {
        // Xử lý xóa thành viên khỏi nhóm
        // ... (như trong mã nguồn bạn đã cung cấp)
    }
    else if (strcmp(command, "SEND") == 0) {
        // Xử lý gửi tin nhắn nhóm
        // ... (như trong mã nguồn bạn đã cung cấp)
    }
    else if (strcmp(command, "PRIVATE") == 0) {
        // Xử lý gửi tin nhắn riêng tư
        // ... (như trong mã nguồn bạn đã cung cấp)
    }
    else if (strcmp(command, "LIST_GROUPS") == 0) {
        // Xử lý liệt kê các nhóm
        // ... (như trong mã nguồn bạn đã cung cấp)
    }
    else if (strcmp(command, "LIST_USERS") == 0) {
        // Xử lý liệt kê người dùng
        // ... (như trong mã nguồn bạn đã cung cấp)
    }
    else if (strcmp(command, "UPLOAD_FILE") == 0) {
        // Xử lý tải lên file
        // ... (như trong mã nguồn bạn đã cung cấp)
    }
    else if (strcmp(command, "DOWNLOAD_FILE") == 0) {
        // Xử lý tải xuống file
        // ... (như trong mã nguồn bạn đã cung cấp)
    }
    else if (strcmp(command, "SEARCH") == 0) {
        // Xử lý tìm kiếm file
        // ... (như trong mã nguồn bạn đã cung cấp)
    }
    else {
        send_response(client_sock, "UnknownCommand\n");
    }

    return 0;
}

// Hàm xử lý kết nối của từng client
void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    // Nhận yêu cầu đăng ký hoặc đăng nhập
    ssize_t bytesReceived = recv(client->sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
        close(client->sock);
        free(client);
        pthread_exit(NULL);
    }
    buffer[bytesReceived] = '\0';

    // Xử lý đăng ký hoặc đăng nhập
    if (strncmp(buffer, "REGISTER", 8) == 0) {
    char username[50], password[50], email[100];
        // Đảm bảo rằng bạn có đủ dữ liệu trước khi sử dụng sscanf
        int scanned = sscanf(buffer + 9, "%49s %49s %99s", username, password, email);
        if (scanned != 3) {
            send_response(client->sock, "RegisterFailed: Invalid data format.\n");
            close(client->sock);
            free(client);
            pthread_exit(NULL);
        }

        // Kiểm tra xem người dùng đã tồn tại chưa
        char query[BUFFER_SIZE];
        snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s'", username);
        MYSQL_RES *res = execute_query(&db, query);
        if (res != NULL && mysql_num_rows(res) > 0) {
            send_response(client->sock, "UserExists\n");
            mysql_free_result(res);
        } else {
            if (res != NULL && res != (MYSQL_RES *)1) {
                mysql_free_result(res);
            }
            // Thêm người dùng vào cơ sở dữ liệu
            snprintf(query, sizeof(query), 
                     "INSERT INTO users (username, password, email, status) VALUES ('%s', '%s', '%s', 'offline')", 
                     username, password, email);
            MYSQL_RES *insert_res = execute_query(&db, query);
            if (insert_res == NULL) {
                send_response(client->sock, "RegisterFailed\n");
            } else {
                // Kiểm tra xem insert_res là (MYSQL_RES *)1 hay không
                if (insert_res == (MYSQL_RES *)1) {
                    // Lấy ID người dùng vừa đăng ký
                    int user_id = mysql_insert_id(db.conn);
                    client->id = user_id;
                    strncpy(client->username, username, sizeof(client->username) - 1);
                    send_response(client->sock, "RegisterSuccess\n");
                    printf("User %s đã đăng ký.\n", username);
                } else {
                    // Trường hợp này không nên xảy ra với INSERT
                    send_response(client->sock, "RegisterFailed\n");
                }
            }
        }
    } 
    else if (strncmp(buffer, "LOGIN", 5) == 0) {
        char username[50], password[50];
        int scanned = sscanf(buffer + 6, "%49s %49s", username, password);
        if (scanned != 2) {
            send_response(client->sock, "LoginFailed: Invalid data format.\n");
            close(client->sock);
            free(client);
            pthread_exit(NULL);
        }

        // Kiểm tra thông tin đăng nhập
        char query[BUFFER_SIZE];
        snprintf(query, sizeof(query), 
                 "SELECT id FROM users WHERE username='%s' AND password='%s'", 
                 username, password);
        MYSQL_RES *res = execute_query(&db, query);
        if (res != NULL && mysql_num_rows(res) > 0) {
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
        } else {
            send_response(client->sock, "LoginFailed\n");
            if (res != NULL) {
                mysql_free_result(res);
            }
            close(client->sock);
            free(client);
            pthread_exit(NULL);
        }
    } 
    else {
        send_response(client->sock, "InvalidCommand\n");
        close(client->sock);
        free(client);
        pthread_exit(NULL);
    }

    // Vòng lặp xử lý các lệnh sau khi đăng nhập hoặc đăng ký thành công
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        bytesReceived = recv(client->sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            // Cập nhật trạng thái offline
            char update_query[BUFFER_SIZE];
            snprintf(update_query, sizeof(update_query), 
                     "UPDATE users SET status='offline' WHERE id=%d", client->id);
            execute_query(&db, update_query);

            printf("User %s đã ngắt kết nối.\n", client->username);
            close(client->sock);
            free(client);
            pthread_exit(NULL);
        }
        buffer[bytesReceived] = '\0';

        // Xử lý các lệnh
        handle_commands(&db, client->sock, client->id, client->username, buffer);
    }

    close(client->sock);
    free(client);
    pthread_exit(NULL);
}

// Hàm lắng nghe trên một cổng và tạo thread để xử lý kết nối
void *listen_on_port(void *arg) {
    int port = *(int *)arg;
    free(arg);

    int server_sock, new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed");
        pthread_exit(NULL);
    }

    // Thiết lập địa chỉ
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Gắn socket với địa chỉ
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        pthread_exit(NULL);
    }

    // Lắng nghe kết nối
    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        close(server_sock);
        pthread_exit(NULL);
    }

    printf("Server đang lắng nghe trên cổng %d...\n", port);

    while (1) {
        if ((new_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        // Tạo đối tượng client
        Client *client = (Client *)malloc(sizeof(Client));
        if (client == NULL) {
            perror("Malloc failed");
            close(new_sock);
            continue;
        }
        client->sock = new_sock;
        client->id = -1;
        memset(client->username, 0, sizeof(client->username));

        // Tạo thread để xử lý client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)client) != 0) {
            perror("pthread_create failed");
            close(new_sock);
            free(client);
            continue;
        }
        pthread_detach(thread_id);
    }

    close(server_sock);
    pthread_exit(NULL);
}

int main() {
    // Kết nối đến cơ sở dữ liệu
    if (connect_db(&db, "localhost", "root", "transyhieu123", "chat_app") != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to connect to database.\n");
        exit(EXIT_FAILURE);
    }

    // Tạo thread để lắng nghe trên MAIN_PORT
    pthread_t main_thread;
    int *main_port = malloc(sizeof(int));
    if (main_port == NULL) {
        perror("Malloc failed");
        exit(EXIT_FAILURE);
    }
    *main_port = MAIN_PORT;

    if (pthread_create(&main_thread, NULL, listen_on_port, (void *)main_port) != 0) {
        perror("pthread_create failed for main_port");
        free(main_port);
        exit(EXIT_FAILURE);
    }

    // Chờ thread kết thúc (không bao giờ)
    pthread_join(main_thread, NULL);

    close_db(&db);
    return 0;
}
