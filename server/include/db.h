// server/include/db.h

#ifndef DB_H
#define DB_H

#include <mysql/mysql.h>

// Cấu trúc để lưu thông tin kết nối MySQL
typedef struct {
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
} DBConnection;

// Hàm để kết nối đến cơ sở dữ liệu
int connect_db(DBConnection *db, const char *host, const char *user, const char *password, const char *dbname);

// Hàm để thực hiện câu lệnh SQL (INSERT, UPDATE, DELETE)
MYSQL_RES *execute_query(DBConnection *db, const char *query);

// Hàm lấy kết quả truy vấn (SELECT)
int fetch_query_result(DBConnection *db, const char *query);

// Hàm đóng kết nối với cơ sở dữ liệu
void close_db(DBConnection *db);

#endif // DB_H
