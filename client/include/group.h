// client/include/group.h

#ifndef GROUP_H
#define GROUP_H

#include <stdbool.h>

// Hàm khởi tạo kết nối nhóm
int init_group(int sock, const char *username);

// Hàm tạo nhóm
void create_group(int sock, const char *group_name);

// Hàm tham gia nhóm
void join_group(int sock, const char *group_name);

// Hàm rời nhóm
void leave_group(int sock, const char *group_name);

// Hàm gửi tin nhắn nhóm
void send_group_message(int sock, const char *group_name, const char *message);

// Hàm thêm thành viên
void add_member(int sock, const char *group_name, const char *member_username);

// Hàm xóa thành viên
void remove_member(int sock, const char *group_name, const char *member_username);

// Hàm liệt kê các nhóm
void list_groups(int sock);

// Hàm liệt kê người dùng
void list_users(int sock);

#endif // GROUP_H
