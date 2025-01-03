// server/src/group.c

#include "../include/group.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100
int create_group(DBConnection *db, const char *group_name, int creator_id)
{
    char query[BUFFER_SIZE];
    int group_id = -1;

    // Tạo nhóm mới
    snprintf(query, sizeof(query),
             "INSERT INTO chat_groups (group_name, creator_id) VALUES ('%s', %d)",
             group_name, creator_id);
    if (execute_query(db, query) == NULL)
    {
        return -1;
    }

    // Lấy ID của nhóm vừa tạo
    snprintf(query, sizeof(query), "SELECT id FROM chat_groups WHERE group_name='%s'", group_name);
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

    // Thêm người tạo vào nhóm với vai trò admin
    snprintf(query, sizeof(query),
             "INSERT INTO group_members (group_id, user_id, role) VALUES (%d, %d, 'admin')",
             group_id, creator_id);
    if (execute_query(db, query) == NULL)
    {
        return -1;
    }

    return group_id;
}

int join_group(DBConnection *db, const char *group_name, int user_id)
{
    char query[BUFFER_SIZE];
    int group_id = -1;

    // Lấy group_id từ tên nhóm
    snprintf(query, sizeof(query), "SELECT id FROM chat_groups WHERE group_name='%s'", group_name);
    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL)
    {
        mysql_free_result(res);
        return -1; // Nhóm không tồn tại
    }
    group_id = atoi(row[0]);
    mysql_free_result(res);

    // Kiểm tra xem người dùng đã là thành viên chưa
    snprintf(query, sizeof(query),
             "SELECT 1 FROM group_members WHERE group_id=%d AND user_id=%d",
             group_id, user_id);
    res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }
    row = mysql_fetch_row(res);
    if (row != NULL)
    {
        mysql_free_result(res);
        return -2; // Đã là thành viên
    }
    mysql_free_result(res);

    // Thêm người dùng vào nhóm với vai trò member
    snprintf(query, sizeof(query),
             "INSERT INTO group_members (group_id, user_id, role) VALUES (%d, %d, 'member')",
             group_id, user_id);
    if (execute_query(db, query) == NULL)
    {
        return -1;
    }

    return group_id;
}

int leave_group(DBConnection *db, const char *group_name, int user_id)
{
    char query[BUFFER_SIZE];
    int group_id = -1;
    int is_admin = 0;

    // Lấy group_id từ tên nhóm
    snprintf(query, sizeof(query), "SELECT id FROM chat_groups WHERE group_name='%s'", group_name);
    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL)
    {
        mysql_free_result(res);
        return -1; // Nhóm không tồn tại
    }
    group_id = atoi(row[0]);
    mysql_free_result(res);

    // Kiểm tra nếu người dùng là admin
    snprintf(query, sizeof(query),
             "SELECT role FROM group_members WHERE group_id=%d AND user_id=%d",
             group_id, user_id);
    res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }
    row = mysql_fetch_row(res);
    if (row == NULL)
    {
        mysql_free_result(res);
        return -1; // Người dùng không phải là thành viên của nhóm
    }
    is_admin = strcmp(row[0], "admin") == 0;
    mysql_free_result(res);

    // Nếu là admin, chuyển quyền admin cho user tham gia gần nhất
    if (is_admin)
    {
        snprintf(query, sizeof(query),
                 "SELECT user_id FROM group_members "
                 "WHERE group_id=%d AND user_id != %d "
                 "ORDER BY joined_at ASC LIMIT 1",
                 group_id, user_id);
        res = execute_query(db, query);
        if (res == NULL)
        {
            return -1;
        }
        row = mysql_fetch_row(res);
        if (row != NULL)
        {
            int new_admin_id = atoi(row[0]);
            mysql_free_result(res);

            // Cập nhật role của user tham gia gần nhất thành admin
            snprintf(query, sizeof(query),
                     "UPDATE group_members SET role='admin' WHERE group_id=%d AND user_id=%d",
                     group_id, new_admin_id);
            if (execute_query(db, query) == NULL)
            {
                return -1;
            }
        }
        else
        {
            // Không còn thành viên nào khác trong nhóm
            mysql_free_result(res);
        }
    }

    // Xóa người dùng khỏi nhóm
    snprintf(query, sizeof(query),
             "DELETE FROM group_members WHERE group_id=%d AND user_id=%d",
             group_id, user_id);
    if (execute_query(db, query) == NULL)
    {
        return -1;
    }

    return 0; // Thành công
}

int add_member_to_group(DBConnection *db, const char *group_name, const char *member_username)
{
    char query[BUFFER_SIZE];
    int group_id = -1;
    int member_id = -1;

    // Lấy group_id từ tên nhóm
    snprintf(query, sizeof(query), "SELECT id FROM chat_groups WHERE group_name='%s'", group_name);
    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL)
    {
        mysql_free_result(res);
        return -1; // Nhóm không tồn tại
    }
    group_id = atoi(row[0]);
    mysql_free_result(res);

    // Lấy user_id của thành viên cần thêm
    snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s'", member_username);
    res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }
    row = mysql_fetch_row(res);
    if (row == NULL)
    {
        mysql_free_result(res);
        return -1; // Người dùng không tồn tại
    }
    member_id = atoi(row[0]);
    mysql_free_result(res);

    // Kiểm tra xem người dùng đã là thành viên chưa
    snprintf(query, sizeof(query),
             "SELECT 1 FROM group_members WHERE group_id=%d AND user_id=%d",
             group_id, member_id);
    res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }
    row = mysql_fetch_row(res);
    if (row != NULL)
    {
        mysql_free_result(res);
        return -2; // Đã là thành viên
    }
    mysql_free_result(res);

    // Thêm người dùng vào nhóm với vai trò member
    snprintf(query, sizeof(query),
             "INSERT INTO group_members (group_id, user_id, role) VALUES (%d, %d, 'member')",
             group_id, member_id);
    if (execute_query(db, query) == NULL)
    {
        return -1;
    }

    return 0; // Thành công
}

int remove_member_from_group(DBConnection *db, const char *group_name, const char *member_username)
{
    char query[BUFFER_SIZE];
    int group_id = -1;
    int member_id = -1;

    // Lấy group_id từ tên nhóm
    snprintf(query, sizeof(query), "SELECT id FROM chat_groups WHERE group_name='%s'", group_name);
    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL)
    {
        mysql_free_result(res);
        return -1; // Nhóm không tồn tại
    }
    group_id = atoi(row[0]);
    mysql_free_result(res);

    // Lấy user_id của thành viên cần xóa
    snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s'", member_username);
    res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }
    row = mysql_fetch_row(res);
    if (row == NULL)
    {
        mysql_free_result(res);
        return -1; // Người dùng không tồn tại
    }
    member_id = atoi(row[0]);
    mysql_free_result(res);

    // Xóa người dùng khỏi nhóm
    snprintf(query, sizeof(query),
             "DELETE FROM group_members WHERE group_id=%d AND user_id=%d",
             group_id, member_id);
    if (execute_query(db, query) == NULL)
    {
        return -1;
    }

    return 0; // Thành công
}

int list_user_groups(DBConnection *db, int user_id, char *groups, size_t group_size)
{
    char query[BUFFER_SIZE];
    snprintf(query, sizeof(query),
             "SELECT group_name FROM chat_groups "
             "JOIN group_members ON chat_groups.id = group_members.group_id "
             "WHERE group_members.user_id = %d",
             user_id);

    printf("Executing query: %s\n", query);

    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL)
    {
        return -1; // Lỗi cơ sở dữ liệu
    }

    MYSQL_ROW row;
    groups[0] = '\0';  // Khởi tạo chuỗi rỗng
    int has_group = 0; // Biến kiểm tra có nhóm nào không
    while ((row = mysql_fetch_row(res)) != NULL)
    {
        has_group = 1;
        // Nối tên nhóm vào chuỗi `groups`
        strncat(groups, row[0], group_size - strlen(groups) - 1);
        strncat(groups, ":", group_size - strlen(groups) - 1);
    }
    mysql_free_result(res);

    if (!has_group)
    {
        strncpy(groups, "NoGroups", group_size - 1); // Gửi thông báo không có nhóm
        groups[group_size - 1] = '\0';               // Đảm bảo chuỗi null-terminated
    }

    return 0; // Thành công
}

int list_all_users(DBConnection *db, char *result, size_t result_size, const char *group_name)
{
    char query[BUFFER_SIZE];

    // Nếu có group_name, lấy danh sách người dùng trong nhóm
    if (group_name != NULL && strlen(group_name) > 0)
    {
        snprintf(query, sizeof(query),
                 "SELECT u.username FROM users u "
                 "JOIN group_members gm ON u.id = gm.user_id "
                 "JOIN chat_groups cg ON gm.group_id = cg.id "
                 "WHERE cg.group_name = '%s'",
                 group_name);
    }
    else
    {
        // Nếu không có group_name, lấy tất cả người dùng
        snprintf(query, sizeof(query), "SELECT username FROM users");
    }

    printf("Executing query: %s\n", query);

    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL)
    {
        strncpy(result, "Không thể lấy danh sách người dùng.\n", result_size);
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
        strncpy(result, group_name && strlen(group_name) > 0 ? "Nhóm không có người dùng nào.\n" : "Không có người dùng nào.\n",
                result_size);
    }
    else
    {
        // Xóa ký tự ':' cuối cùng nếu có
        if (offset > 0 && result[offset - 1] == ':')
        {
            result[offset - 1] = '\0';
        }
    }

    return 0;
}

int send_group_message(DBConnection *db, Client *client, const char *group_name, const char *message, Client *clients[])
{
    int sender_id = client->id;
    char query[BUFFER_SIZE];
    snprintf(query, sizeof(query), "SELECT id FROM chat_groups WHERE group_name='%s'", group_name);
    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL)
    {
        mysql_free_result(res);
        return -1; // Nhóm không tồn tại
    }
    int group_id = atoi(row[0]);
    mysql_free_result(res);

    // Kiểm tra nếu người gửi là thành viên của nhóm
    snprintf(query, sizeof(query),
             "SELECT 1 FROM group_members WHERE group_id=%d AND user_id=%d",
             group_id, sender_id);
    res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }
    row = mysql_fetch_row(res);
    if (row == NULL)
    {
        mysql_free_result(res);
        return -1; // Người gửi không phải thành viên
    }
    mysql_free_result(res);

    // Lưu tin nhắn vào cơ sở dữ liệu
    snprintf(query, sizeof(query),
             "INSERT INTO messages (sender_id, group_id, message) VALUES (%d, %d, '%s')",
             sender_id, group_id, message);
    if (execute_query(db, query) == NULL)
    {
        return -1;
    }

    // Gửi tin nhắn tới tất cả thành viên trong nhóm (ngoại trừ người gửi)
    // Bạn cần triển khai danh sách các client đang kết nối để tìm và gửi tin nhắn
    // Gửi tin nhắn tới tất cả thành viên trong nhóm (ngoại trừ người gửi)
    snprintf(query, sizeof(query),
             "SELECT user_id FROM group_members WHERE group_id=%d AND user_id!=%d",
             group_id, sender_id);
    res = execute_query(db, query);
    if (res == NULL)
    {
        return -1;
    }

    while ((row = mysql_fetch_row(res)) != NULL)
    {
        int member_id = atoi(row[0]);

        // Kiểm tra nếu thành viên đang trực tuyến
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] != NULL && clients[i]->id == member_id)
            {
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "MESSAGE: %s: %s", client->username, message);
                send_response(clients[i]->sock, response);
            }
        }
    }

    mysql_free_result(res);

    return 0;
}

int list_group_messages(DBConnection *db, const char *group_name, char *result, size_t result_size)
{
    char query[BUFFER_SIZE];
    snprintf(query, sizeof(query),
             "SELECT u.username, m.message, m.created_at FROM messages m "
             "JOIN users u ON m.sender_id = u.id "
             "JOIN chat_groups g ON m.group_id = g.id "
             "WHERE g.group_name = '%s' ORDER BY m.created_at ASC",
             group_name);
    MYSQL_RES *res = execute_query(db, query);
    if (res == NULL)
    {
        strncpy(result, "Không thể lấy tin nhắn nhóm.\n", result_size);
        return -1;
    }

    MYSQL_ROW row;
    size_t offset = 0;
    while ((row = mysql_fetch_row(res)) != NULL)
    {
        offset += snprintf(result + offset, result_size - offset, "[%s] %s: %s\n", row[2], row[0], row[1]);
        if (offset >= result_size)
            break;
    }
    mysql_free_result(res);

    if (offset == 0)
    {
        strncpy(result, "Không có tin nhắn trong nhóm này.\n", result_size);
    }

    return 0;
}
