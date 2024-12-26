// server/include/file.h

#ifndef SERVER_FILE_H
#define SERVER_FILE_H

#include "../include/db.h"

// Hàm tải lên file
int upload_file(DBConnection *db, int sender_id, const char *receiver_username, const char *file_name, const char *file_type, const char *file_path, int is_group);

// Hàm tải xuống file
int download_file(DBConnection *db, int sender_id, const char *file_name, char *file_path, size_t path_size);

// Hàm tìm kiếm file
int search_files(DBConnection *db, const char *query, char *result, size_t result_size);

// Hàm tải lên thư mục
int upload_directory(DBConnection *db, int sender_id, const char *receiver_username, const char *dir_path, int is_group);

// Hàm tải xuống thư mục
int download_directory(DBConnection *db, int sender_id, const char *dir_name, char *save_path, size_t path_size);

#endif // SERVER_FILE_H
