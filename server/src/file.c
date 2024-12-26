// server/src/file.c

#include "../include/file.h"
#include "../include/utils.h"
#include "../include/group.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

// Hàm tải lên file
int upload_file(DBConnection *db, int sender_id, const char *receiver_username, const char *file_name, const char *file_type, const char *file_path, int is_group) {
    char query[BUFFER_SIZE];
    int receiver_id = -1;
    int group_id = -1;

    if (is_group) {
        // Lấy group_id từ tên nhóm
        snprintf(query, sizeof(query), "SELECT id FROM chat_groups WHERE group_name='%s'", receiver_username);
        MYSQL_RES *res = execute_query(db, query);
        if (res == NULL) {
            return -1;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row == NULL) {
            mysql_free_result(res);
            return -1;
        }
        group_id = atoi(row[0]);
        mysql_free_result(res);
    } else {
        // Lấy user_id từ tên người nhận
        snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s'", receiver_username);
        MYSQL_RES *res = execute_query(db, query);
        if (res == NULL) {
            return -1;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row == NULL) {
            mysql_free_result(res);
            return -1;
        }
        receiver_id = atoi(row[0]);
        mysql_free_result(res);
    }

    // Lưu thông tin file vào cơ sở dữ liệu
    if (is_group) {
        snprintf(query, sizeof(query),
                 "INSERT INTO files (sender_id, group_id, file_name, file_type, file_path) VALUES (%d, %d, '%s', '%s', '%s')",
                 sender_id, group_id, file_name, file_type, file_path);
    } else {
        snprintf(query, sizeof(query),
                 "INSERT INTO files (sender_id, receiver_id, file_name, file_type, file_path) VALUES (%d, %d, '%s', '%s', '%s')",
                 sender_id, receiver_id, file_name, file_type, file_path);
    }

    if (execute_query(db, query) == NULL) {
        return -1;
    }

    return 0; // Thành công
}

// Hàm tải xuống file
int download_file(DBConnection *db, int sender_id, const char *file_name, char *file_path, size_t path_size) {
    char query[BUFFER_SIZE];
    snprintf(query,
             sizeof(query),
             "SELECT file_path FROM files WHERE file_name='%s' AND (receiver_id=%d OR group_id IN (SELECT group_id FROM group_members WHERE user_id=%d))",
             file_name, sender_id, sender_id);
    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL) {
        strncpy(file_path, "File not found.\n", path_size);
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL) {
        strncpy(file_path, "File not found.\n", path_size);
        mysql_free_result(res);
        return -1;
    }
    strncpy(file_path, row[0], path_size - 1);
    file_path[path_size - 1] = '\0';
    mysql_free_result(res);

    return 0;
}

// Hàm tìm kiếm file
int search_files(DBConnection *db, const char *query_str, char *result, size_t result_size) {
    char query[BUFFER_SIZE];
    snprintf(query,
             sizeof(query),
             "SELECT file_name FROM files WHERE file_name LIKE '%%%s%%'",
             query_str);
    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL) {
        strncpy(result, "Không thể tìm kiếm file.\n", result_size);
        return -1;
    }

    MYSQL_ROW row;
    size_t offset = 0;
    while ((row = mysql_fetch_row(res)) != NULL) {
        offset += snprintf(result + offset, result_size - offset, "%s\n", row[0]);
        if (offset >= result_size) break;
    }
    mysql_free_result(res);

    if (offset == 0) {
        strncpy(result, "Không tìm thấy file nào phù hợp.\n", result_size);
    }

    return 0;
}

