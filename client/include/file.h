// client/include/file.h

#ifndef FILE_H
#define FILE_H

#include <stdbool.h>

// Hàm khởi tạo kết nối file
int init_file(int sock, const char *username);

// Hàm tải lên file
void upload_file(int sock, const char *file_path, const char *receiver, bool is_group);

// Hàm tải xuống file
void download_file(int sock, const char *file_name, const char *save_path);

// Hàm tìm kiếm file
void search_files(int sock, const char *query);

// Hàm tải lên thư mục
void upload_directory(int sock, const char *dir_path, const char *receiver, bool is_group);

// Hàm tải xuống thư mục
void download_directory(int sock, const char *dir_name, const char *save_path);

#endif // FILE_H
