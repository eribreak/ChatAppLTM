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

int init_file(int sock, const char *username)
{
    // Không cần khởi tạo gì đặc biệt tại client cho file
    return 0;
}

void upload_file(int sock, const char *file_path, const char *receiver, bool is_group)
{
    // Tạo buffer để chứa toàn bộ dữ liệu (metadata + file)
    char buffer[BUFFER_SIZE * 2]; // Dành không gian cho metadata và nội dung file
    int offset = 0;

    // Mở file để đọc dữ liệu
    FILE *fp = fopen(file_path, "rb");
    if (!fp)
    {
        printf("Không thể mở file %s để tải lên.\n", file_path);
        return;
    }

    // Lấy kích thước file
    fseek(fp, 0, SEEK_END);
    uint64_t filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Tạo metadata: loại upload + người nhận + đường dẫn file + kích thước file
    if (is_group)
    {
        offset += snprintf(buffer, sizeof(buffer), "UPLOAD_GROUP %s %llu %s\n", receiver, filesize, file_path);
    }
    else
    {
        offset += snprintf(buffer, sizeof(buffer), "UPLOAD_PRIVATE %s %llu %s\n", receiver, filesize, file_path);
    }

    // Đọc dữ liệu file vào buffer (sau metadata)
    size_t bytesRead;
    while ((bytesRead = fread(buffer + offset, 1, sizeof(buffer) - offset, fp)) > 0)
    {
        offset += bytesRead;
    }
    fclose(fp);

    // Gửi toàn bộ buffer (bao gồm metadata và file) đi trong một lần
    if (send(sock, buffer, offset, 0) < 0)
    {
        printf("Lỗi khi gửi dữ liệu.\n");
        return;
    }

    printf("Đã tải lên file %s thành công.\n", file_path);
}

void download_file(int sock, const char *file_name, const char *save_path)
{
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "DOWNLOAD %s %s", file_name, save_path);
    send(sock, buffer, strlen(buffer), 0);

    // // Nhận phản hồi từ server
    // char response[BUFFER_SIZE];
    // ssize_t bytesReceived = recv(sock, response, sizeof(response) - 1, 0);

    // if (bytesReceived > 0)
    // {
    //     response[bytesReceived] = '\0';
    //     // Loại bỏ ký tự dòng mới nếu có
    //     response[strcspn(response, "\n")] = 0;
    //     printf("Server: %s\n", response);

    //     // Nếu server báo lỗi hoặc file không tồn tại
    //     if (strncmp(response, "ERROR", 5) == 0)
    //     {
    //         printf("Lỗi từ server: %s\n", response);
    //         return;
    //     }

    //     // Nhận nội dung file
    //     FILE *fp = fopen(save_path, "wb");
    //     if (!fp)
    //     {
    //         printf("Không thể tạo file %s để lưu.\n", save_path);
    //         return;
    //     }

    //     printf("hi\n");
    //     // Vòng lặp nhận và ghi dữ liệu vào file
    //     ssize_t bytes;
    //     while ((bytes = recv(sock, buffer, sizeof(buffer), 0)) > 0)
    //     {
    //         fwrite(buffer, 1, bytes, fp);
    //     }

    //     if (bytes < 0)
    //     {
    //         printf("Lỗi khi nhận dữ liệu từ server.\n");
    //     }
    //     else
    //     {
    //         printf("Đã tải xuống file %s thành công.\n", file_name);
    //     }

    //     fclose(fp);
    // }
    // else
    // {
    //     printf("Không nhận được phản hồi từ server.\n");
    // }
}

void save_file(const char *file_content, const char *save_path)
{
    // Mở file để ghi dữ liệu (chế độ ghi nhị phân)
    FILE *fp = fopen(save_path, "wb");
    if (!fp)
    {
        printf("Không thể tạo file %s để lưu.\n", save_path);
        return;
    }

    // Ghi nội dung file vào file
    size_t content_length = strlen(file_content); // Tính độ dài nội dung
    size_t bytes_written = fwrite(file_content, sizeof(char), content_length, fp);
    if (bytes_written < content_length)
    {
        printf("Đã xảy ra lỗi khi ghi file %s.\n", save_path);
        fclose(fp);
        return;
    }

    printf("File đã được lưu thành công tại %s.\n", save_path);

    // Đóng file
    fclose(fp);
}

void search_files(int sock, const char *query)
{
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "SEARCH %s", query);
    send(sock, buffer, strlen(buffer), 0);

    // Nhận kết quả tìm kiếm
    printf("Kết quả tìm kiếm:\n");
}

void upload_directory(int sock, const char *dir_path, const char *receiver, bool is_group)
{
    // Gửi lệnh tải lên thư mục
    char buffer[BUFFER_SIZE];
    if (is_group)
    {
        snprintf(buffer, sizeof(buffer), "UPLOAD_DIR_GROUP %s %s", receiver, dir_path);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "UPLOAD_DIR_PRIVATE %s %s", receiver, dir_path);
    }
    send(sock, buffer, strlen(buffer), 0);

    // Đệ quy tải lên các file và thư mục con
    DIR *d = opendir(dir_path);
    if (!d)
    {
        printf("Không thể mở thư mục %s.\n", dir_path);
        return;
    }

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL)
    {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
            continue;
        char path[BUFFER_SIZE];
        snprintf(path, sizeof(path), "%s/%s", dir_path, dir->d_name);

        struct stat st;
        stat(path, &st);
        if (S_ISDIR(st.st_mode))
        {
            // Đệ quy tải lên thư mục con
            upload_directory(sock, path, receiver, is_group);
        }
        else if (S_ISREG(st.st_mode))
        {
            // Tải lên file
            upload_file(sock, path, receiver, is_group);
        }
    }
    closedir(d);
    printf("Đã tải lên thư mục %s thành công.\n", dir_path);
}

void download_directory(int sock, const char *dir_name, const char *save_path)
{
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "DOWNLOAD_DIR %s", dir_name);
    send(sock, buffer, strlen(buffer), 0);

    // Nhận dữ liệu thư mục
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0)
        {
            printf("Lỗi khi nhận dữ liệu thư mục.\n");
            break;
        }
        buffer[bytesReceived] = '\0';

        if (strcmp(buffer, "END_OF_DIR") == 0)
        {
            break;
        }

        // Xử lý tạo thư mục hoặc nhận file
        if (strncmp(buffer, "MKDIR ", 6) == 0)
        {
            char dir_to_create[BUFFER_SIZE];
            sscanf(buffer + 6, "%s", dir_to_create);
            mkdir(dir_to_create, 0777);
            printf("Đã tạo thư mục %s.\n", dir_to_create);
        }
        else if (strncmp(buffer, "FILE ", 5) == 0)
        {
            char file_name[BUFFER_SIZE];
            sscanf(buffer + 5, "%s", file_name);
            char save_file_path[BUFFER_SIZE];
            snprintf(save_file_path, sizeof(save_file_path), "%s/%s", save_path, file_name);
            download_file(sock, file_name, save_file_path);
        }
    }
    printf("Đã tải xuống thư mục %s thành công.\n", dir_name);
}
