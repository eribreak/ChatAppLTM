// client/src/group.c

#include "../include/group.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int init_group(int sock, const char *username) {
    // Không cần khởi tạo gì đặc biệt tại client cho nhóm
    return 0;
}

void create_group(int sock, const char *group_name) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "CREATE_GROUP %s", group_name);
    send(sock, buffer, strlen(buffer), 0);
}

void join_group(int sock, const char *group_name) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "JOIN_GROUP %s", group_name);
    send(sock, buffer, strlen(buffer), 0);
}

void leave_group(int sock, const char *group_name) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "LEAVE_GROUP %s", group_name);
    send(sock, buffer, strlen(buffer), 0);
}

void send_group_message(int sock, const char *group_name, const char *message) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "SEND %s %s", group_name, message);
    send(sock, buffer, strlen(buffer), 0);
}

void add_member(int sock, const char *group_name, const char *member_username) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "ADD_MEMBER %s %s", group_name, member_username);
    send(sock, buffer, strlen(buffer), 0);
}

void remove_member(int sock, const char *group_name, const char *member_username) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "REMOVE_MEMBER %s %s", group_name, member_username);
    send(sock, buffer, strlen(buffer), 0);
}

void list_groups(int sock) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "LIST_GROUPS");
    send(sock, buffer, strlen(buffer), 0);
}

void list_users(int sock) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "LIST_USERS");
    send(sock, buffer, strlen(buffer), 0);
}
