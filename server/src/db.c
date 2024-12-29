// server/src/db.c

#include "../include/db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Hàm kết nối đến cơ sở dữ liệu
int connect_db(DBConnection *db, const char *host, const char *user, const char *password, const char *dbname) {
    db->conn = mysql_init(NULL);
    if (db->conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        return EXIT_FAILURE;
    }

    if (mysql_real_connect(db->conn, host, user,password, dbname, 0, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed: %s\n", mysql_error(db->conn));
        mysql_close(db->conn);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// Hàm thực thi câu lệnh SQL (INSERT, UPDATE, DELETE)
MYSQL_RES *execute_query(DBConnection *db, const char *query) {
    if (mysql_query(db->conn, query)) {
        fprintf(stderr, "MySQL query error: %s\n", mysql_error(db->conn));
        return NULL;
    }
    MYSQL_RES *res = mysql_store_result(db->conn);
    if (res == NULL) {
        if (mysql_field_count(db->conn) == 0) {
            // Truy vấn không trả về dữ liệu (non-SELECT), thành công
            return (MYSQL_RES *)1; // Đánh dấu thành công
        } else {
            // Lỗi khi lấy kết quả
            fprintf(stderr, "MySQL store result failed: %s\n", mysql_error(db->conn));
            return NULL;
        }
    }
    return res;
}

// Hàm lấy kết quả truy vấn (SELECT)
int fetch_query_result(DBConnection *db, const char *query) {
    if (mysql_query(db->conn, query)) {
        fprintf(stderr, "QUERY FAILED: %s\n", mysql_error(db->conn));
        return EXIT_FAILURE;
    }

    db->res = mysql_store_result(db->conn);
    if (db->res == NULL) {
        fprintf(stderr, "STORE RESULT FAILED: %s\n", mysql_error(db->conn));
        return EXIT_FAILURE;
    }

    // In kết quả truy vấn ra (nếu cần thiết)
    /*
    while ((db->row = mysql_fetch_row(db->res)) != NULL) {
        for (int i = 0; i < mysql_num_fields(db->res); i++) {
            printf("%s ", db->row[i] ? db->row[i] : "NULL");
        }
        printf("\n");
    }
    */
    return EXIT_SUCCESS;
}

// Hàm đóng kết nối cơ sở dữ liệu
void close_db(DBConnection *db) {
    if (db->conn) {
        mysql_close(db->conn);
    }
}
