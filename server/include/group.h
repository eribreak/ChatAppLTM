// server/include/group.h

#ifndef SERVER_GROUP_H
#define SERVER_GROUP_H

#include "../include/db.h"

// Hàm tạo nhóm mới
int create_group(DBConnection *db, const char *group_name, int creator_id);

// Hàm tham gia nhóm
int join_group(DBConnection *db, const char *group_name, int user_id);

// Hàm rời nhóm
int leave_group(DBConnection *db, const char *group_name, int user_id);

// Hàm thêm thành viên vào nhóm
int add_member_to_group(DBConnection *db, const char *group_name, const char *member_username);

// Hàm xóa thành viên khỏi nhóm
int remove_member_from_group(DBConnection *db, const char *group_name, const char *member_username);

// Hàm liệt kê các nhóm của người dùng
int list_user_groups(DBConnection *db, int user_id, char *result, size_t result_size);

// Hàm liệt kê người dùng
int list_all_users(DBConnection *db, char *result, size_t result_size);

// Hàm gửi tin nhắn nhóm
int send_group_message(DBConnection *db, int sender_id, const char *group_name, const char *message);

// Hàm liệt kê tin nhắn nhóm
int list_group_messages(DBConnection *db, const char *group_name, char *result, size_t result_size);

#endif // SERVER_GROUP_H
