// client/src/file.c

#include "../include/file.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

int init_file(int sock, const char *username) {
    // Không cần khởi tạo gì đặc biệt tại client cho file
    return 0;
}

void upload_file(int sock, const char *file_path, const char *receiver, bool is_group) {
    char buffer[BUFFER_SIZE];
    if (is_group) {
        snprintf(buffer, sizeof(buffer), "UPLOAD_GROUP %s %s", receiver, file_path);
    } else {
        snprintf(buffer, sizeof(buffer), "UPLOAD_PRIVATE %s %s", receiver, file_path);
    }
    send(sock, buffer, strlen(buffer), 0);

    // Gửi file
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        printf("Không thể mở file %s để tải lên.\n", file_path);
        return;
    }

    // Lấy kích thước file
    fseek(fp, 0, SEEK_END);
    uint64_t filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint64_t filesize_net = htonll(filesize);
    send_all(sock, &filesize_net, sizeof(filesize_net));

    // Gửi nội dung file
    char file_buffer[BUFFER_SIZE];
    size_t bytesRead;
    while ((bytesRead = fread(file_buffer, 1, sizeof(file_buffer), fp)) > 0) {
        if (send_all(sock, file_buffer, bytesRead) != 0) {
            printf("Lỗi khi gửi dữ liệu file.\n");
            fclose(fp);
            return;
        }
    }
    fclose(fp);
    printf("Đã tải lên file %s thành công.\n", file_path);
}

void download_file(int sock, const char *file_name, const char *save_path) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "DOWNLOAD %s", file_name);
    send(sock, buffer, strlen(buffer), 0);

    // Nhận kích thước file
    uint64_t filesize_net;
    if (recv_all(sock, &filesize_net, sizeof(filesize_net)) <= 0) {
        printf("Lỗi khi nhận kích thước file.\n");
        return;
    }
    uint64_t filesize = ntohll(filesize_net);

    // Nhận nội dung file
    FILE *fp = fopen(save_path, "wb");
    if (!fp) {
        printf("Không thể tạo file %s để lưu.\n", save_path);
        return;
    }

    char file_buffer[BUFFER_SIZE];
    uint64_t totalReceived = 0;
    while (totalReceived < filesize) {
        size_t bytesToReceive = (filesize - totalReceived) < BUFFER_SIZE ? (filesize - totalReceived) : BUFFER_SIZE;
        ssize_t bytesReceived = recv_all(sock, file_buffer, bytesToReceive);
        if (bytesReceived <= 0) {
            printf("Lỗi khi nhận dữ liệu file.\n");
            fclose(fp);
            return;
        }
        fwrite(file_buffer, 1, bytesReceived, fp);
        totalReceived += bytesReceived;
    }
    fclose(fp);
    printf("Đã tải xuống file %s thành công.\n", file_name);
}

void search_files(int sock, const char *query) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "SEARCH %s", query);
    send(sock, buffer, strlen(buffer), 0);

    // Nhận kết quả tìm kiếm
    printf("Kết quả tìm kiếm:\n");
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            break;
        }
        buffer[bytesReceived] = '\0';
        if (strcmp(buffer, "END_OF_RESULTS") == 0) {
            break;
        }
        printf("%s\n", buffer);
    }
}

void upload_directory(int sock, const char *dir_path, const char *receiver, bool is_group) {
    // Gửi lệnh tải lên thư mục
    char buffer[BUFFER_SIZE];
    if (is_group) {
        snprintf(buffer, sizeof(buffer), "UPLOAD_DIR_GROUP %s %s", receiver, dir_path);
    } else {
        snprintf(buffer, sizeof(buffer), "UPLOAD_DIR_PRIVATE %s %s", receiver, dir_path);
    }
    send(sock, buffer, strlen(buffer), 0);

    // Đệ quy tải lên các file và thư mục con
    DIR *d = opendir(dir_path);
    if (!d) {
        printf("Không thể mở thư mục %s.\n", dir_path);
        return;
    }

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
            continue;
        char path[BUFFER_SIZE];
        snprintf(path, sizeof(path), "%s/%s", dir_path, dir->d_name);

        struct stat st;
        stat(path, &st);
        if (S_ISDIR(st.st_mode)) {
            // Đệ quy tải lên thư mục con
            upload_directory(sock, path, receiver, is_group);
        } else if (S_ISREG(st.st_mode)) {
            // Tải lên file
            upload_file(sock, path, receiver, is_group);
        }
    }
    closedir(d);
    printf("Đã tải lên thư mục %s thành công.\n", dir_path);
}

void download_directory(int sock, const char *dir_name, const char *save_path) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "DOWNLOAD_DIR %s", dir_name);
    send(sock, buffer, strlen(buffer), 0);

    // Nhận dữ liệu thư mục
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            printf("Lỗi khi nhận dữ liệu thư mục.\n");
            break;
        }
        buffer[bytesReceived] = '\0';

        if (strcmp(buffer, "END_OF_DIR") == 0) {
            break;
        }

        // Xử lý tạo thư mục hoặc nhận file
        if (strncmp(buffer, "MKDIR ", 6) == 0) {
            char dir_to_create[BUFFER_SIZE];
            sscanf(buffer + 6, "%s", dir_to_create);
            mkdir(dir_to_create, 0777);
            printf("Đã tạo thư mục %s.\n", dir_to_create);
        } else if (strncmp(buffer, "FILE ", 5) == 0) {
            char file_name[BUFFER_SIZE];
            sscanf(buffer + 5, "%s", file_name);
            char save_file_path[BUFFER_SIZE];
            snprintf(save_file_path, sizeof(save_file_path), "%s/%s", save_path, file_name);
            download_file(sock, file_name, save_file_path);
        }
    }
    printf("Đã tải xuống thư mục %s thành công.\n", dir_name);
}
