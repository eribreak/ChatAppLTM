// server/src/file.c

#include "../include/file.h"
#include "../include/utils.h"
#include "../include/group.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

// Hàm tạo thư mục nếu chưa tồn tại
int create_directory(const char *path)
{
    struct stat st = {0};
    if (stat(path, &st) == -1)
    {
        return mkdir(path, 0700); // Tạo thư mục với quyền truy cập
    }
    return 0;
}

// Hàm lưu nội dung file vào thư mục
int save_file(const char *file_path, const char *content, int content_size)
{
    int fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        return -1; // Không thể tạo file
    }

    ssize_t written = write(fd, content, content_size);
    if (written != content_size)
    {
        close(fd);
        return -1; // Lỗi khi ghi nội dung
    }

    close(fd);
    return 0; // Thành công
}

// Hàm tải lên file
int upload_file(DBConnection *db, int sender_id, const char *receiver_username, const char *file_name, const char *file_type, const char *file_content, int file_size, int is_group)
{
    char query[BUFFER_SIZE];
    int receiver_id = -1;
    int group_id = -1;
    int file_id = -1;

    if (is_group == 1)
    {
        // Lấy group_id từ tên nhóm
        snprintf(query, sizeof(query), "SELECT id FROM chat_groups WHERE group_name='%s'", receiver_username);
        MYSQL_RES *res = execute_query(db, query);
        if (res == NULL)
        {
            return -1;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row == NULL)
        {
            mysql_free_result(res);
            return -1;
        }
        group_id = atoi(row[0]);
        mysql_free_result(res);
    }
    else
    {
        // Lấy user_id từ tên người nhận
        snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s'", receiver_username);
        MYSQL_RES *res = execute_query(db, query);
        if (res == NULL)
        {
            return -1;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row == NULL)
        {
            mysql_free_result(res);
            return -1;
        }
        receiver_id = atoi(row[0]);
        mysql_free_result(res);
    }

    // Lưu thông tin file vào cơ sở dữ liệu mà không cần file_path
    if (is_group)
    {
        snprintf(query, sizeof(query),
                 "INSERT INTO files (sender_id, group_id, file_name, file_type) VALUES (%d, %d, '%s', '%s')",
                 sender_id, group_id, file_name, file_type);
    }
    else
    {
        snprintf(query, sizeof(query),
                 "INSERT INTO files (sender_id, receiver_id, file_name, file_type) VALUES (%d, %d, '%s', '%s')",
                 sender_id, receiver_id, file_name, file_type);
    }

    // Thực hiện truy vấn và lấy ID của file đã lưu
    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }

    // Sau khi thực hiện câu lệnh INSERT
    snprintf(query, sizeof(query), "SELECT LAST_INSERT_ID()");
    res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }

    printf("hi\n");
    printf("%s\n", query);
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL)
    {
        return -1;
    }

    file_id = atoi(row[0]); // Giả sử ID của file được trả về là ID của dòng mới
    mysql_free_result(res);

    // Nếu không lấy được ID file, trả về lỗi
    if (file_id == -1)
    {
        return -1;
    }

    // Tạo thư mục lưu file nếu chưa có
    create_directory("public");
    create_directory("public/files");

    // Tạo đường dẫn lưu file trong thư mục public
    char saved_file_path[BUFFER_SIZE];
    snprintf(saved_file_path, sizeof(saved_file_path), "public/files/%d_%s", file_id, file_name); // Tên file: file_name + id

    // Lưu nội dung file vào thư mục public
    if (save_file(saved_file_path, file_content, file_size) != 0)
    {
        return -1; // Không thể lưu file
    }

    // Cập nhật lại đường dẫn file vào cơ sở dữ liệu
    snprintf(query, sizeof(query), "UPDATE files SET file_path='%s' WHERE id=%d", saved_file_path, file_id);
    if (execute_query(db, query) == NULL)
    {
        return -1; // Lỗi cập nhật đường dẫn file
    }

    return 0; // Thành công
}

// Hàm tải xuống file
int download_file(DBConnection *db, int sender_id, const char *file_name, char *file_content)
{
    char query[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    char file_path[BUFFER_SIZE];
    snprintf(query,
             sizeof(query),
             "SELECT file_path FROM files WHERE file_name='%s' AND (receiver_id=%d OR group_id IN (SELECT group_id FROM group_members WHERE user_id=%d))",
             file_name, sender_id, sender_id);
    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL)
    {
        strcpy(file_path, "File not found.\n");
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL)
    {
        strcpy(file_path, "File not found.\n");
        mysql_free_result(res);
        return -1;
    }
    strcpy(file_path, row[0]);

    // Mở file để đọc dữ liệu
    FILE *fp = fopen(file_path, "rb");
    if (!fp)
    {
        printf("Không thể mở file %s để tải lên.\n", file_path);
        return -1;
    }

    mysql_free_result(res);

    size_t bytesRead;
    int offset = 0;
    // Đọc dữ liệu file vào buffer
    while ((bytesRead = fread(buffer + offset, 1, sizeof(buffer) - offset, fp)) > 0)
    {
        offset += bytesRead;
    }
    fclose(fp);

    strcpy(file_content, buffer);

    return 0;
}

int search_files(DBConnection *db, const char *query_str, int client_id, char *result, size_t result_size)
{
    char query[BUFFER_SIZE];
    if (query_str == NULL || strlen(query_str) == 0) // Không có điều kiện tìm kiếm
    {
        snprintf(query, sizeof(query),
                 "SELECT file_name FROM files WHERE receiver_id = %d OR sender_id = %d",
                 client_id, client_id);
    }
    else // Có điều kiện tìm kiếm
    {
        snprintf(query, sizeof(query),
                 "SELECT file_name FROM files WHERE (receiver_id = %d OR sender_id = %d) AND file_name LIKE '%%%s%%'",
                 client_id, client_id, query_str);
    }

    printf("Query: %s\n", query);

    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL)
    {
        strncpy(result, "Không thể tìm kiếm file.\n", result_size);
        return -1;
    }

    MYSQL_ROW row;
    size_t offset = 0;
    while ((row = mysql_fetch_row(res)) != NULL)
    {
        offset += snprintf(result + offset, result_size - offset, "%s:", row[0]);
        if (offset >= result_size)
            break;
    }
    mysql_free_result(res);

    if (offset == 0)
    {
        strncpy(result, "Không tìm thấy file nào phù hợp.\n", result_size);
    }

    return 0;
}
