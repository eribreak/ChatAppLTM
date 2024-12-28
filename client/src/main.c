#include <gtk/gtk.h>
#include "../include/chat.h"
#include "../include/group.h"
#include "../include/file.h"
#include "../include/utils.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>

#define BUFFER_SIZE 8086

int sock;
GtkWidget *main_window;
GtkWidget *username_entry;
GtkWidget *password_entry;
GtkWidget *email_entry;
char current_username[50] = ""; // Biến toàn cục lưu username
GtkTextBuffer *chat_buffer;
GtkWidget *chat_text_view;

// Forward declaration of functions
void create_main_menu();
void show_user_list(GtkWidget *widget, gpointer data);
void show_group_list(GtkWidget *widget, gpointer data);
void on_file_sharing_button_clicked(GtkWidget *widget, gpointer data);
void *receive_chat_messages(void *arg);

int connect_to_server(const char *ip, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(sock);
        return -1;
    }

    return sock;
}

void send_message(GtkWidget *button __attribute__((unused)), gpointer data)
{
    GtkWidget **widgets = (GtkWidget **)data;
    GtkWidget *message_entry = widgets[0];
    GtkWidget *chat_text_view = widgets[1];
    char *recipient = (char *)widgets[2];

    if (!GTK_IS_ENTRY(message_entry))
    {
        g_warning("message_entry is not a GtkEntry!");
        return;
    }

    const char *message = gtk_entry_get_text(GTK_ENTRY(message_entry));
    if (strlen(message) == 0)
    {
        g_warning("Empty message, not sending.");
        return;
    }

    if (recipient == NULL)
    {
        g_warning("Recipient is NULL, cannot send message.");
        return;
    }

    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "PRIVATE %s: %s", recipient, message);
    if (send(sock, buffer, strlen(buffer), 0) < 0)
    {
        perror("Failed to send message");
        return;
    }

    GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_text_view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(text_buffer, &end);
    gtk_text_buffer_insert(text_buffer, &end, "\nMe: ", -1);
    gtk_text_buffer_insert(text_buffer, &end, message, -1);
    gtk_text_buffer_insert(text_buffer, &end, "\n", -1);

    gtk_entry_set_text(GTK_ENTRY(message_entry), "");
}

void send_group_message_gtk(GtkWidget *button __attribute__((unused)), gpointer data)
{
    GtkWidget **widgets = (GtkWidget **)data;
    GtkWidget *message_entry = widgets[0];
    GtkWidget *chat_text_view = widgets[1];
    char *group_name = (char *)widgets[2];

    if (!GTK_IS_ENTRY(message_entry))
    {
        g_warning("message_entry is not a GtkEntry!");
        return;
    }

    const char *message = gtk_entry_get_text(GTK_ENTRY(message_entry));
    if (strlen(message) == 0)
    {
        g_warning("Empty message, not sending.");
        return;
    }

    if (group_name == NULL)
    {
        g_warning("Group name is NULL, cannot send message.");
        return;
    }

    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "SEND %s %s", group_name, message);
    if (send(sock, buffer, strlen(buffer), 0) < 0)
    {
        perror("Failed to send group message");
        return;
    }

    GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_text_view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(text_buffer, &end);
    gtk_text_buffer_insert(text_buffer, &end, "Me (to group): ", -1);
    gtk_text_buffer_insert(text_buffer, &end, message, -1);
    gtk_text_buffer_insert(text_buffer, &end, "\n", -1);

    gtk_entry_set_text(GTK_ENTRY(message_entry), "");
}

void open_chat_window(const char *recipient)
{
    if (recipient == NULL)
    {
        g_warning("Recipient is NULL, cannot open chat window.");
        return;
    }

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), recipient);
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    chat_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_text_view), FALSE);
    chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_text_view));
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), chat_text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *message_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), message_entry, TRUE, TRUE, 0);

    GtkWidget *send_button = gtk_button_new_with_label("Send");
    gtk_box_pack_start(GTK_BOX(hbox), send_button, FALSE, FALSE, 0);

    char *recipient_copy = g_strdup(recipient);
    GtkWidget **widgets = g_malloc(sizeof(GtkWidget *) * 3);
    widgets[0] = message_entry;
    widgets[1] = chat_text_view;
    widgets[2] = (GtkWidget *)recipient_copy;

    g_signal_connect(send_button, "clicked", G_CALLBACK(send_message), widgets);

    gtk_widget_show_all(window);

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, &receive_chat_messages, NULL);
}

void open_group_chat_window(const char *group_name)
{
    if (group_name == NULL)
    {
        g_warning("Group name is NULL, cannot open group chat window.");
        return;
    }

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), group_name);
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *chat_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_text_view), FALSE);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), chat_text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *message_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), message_entry, TRUE, TRUE, 0);

    GtkWidget *send_button = gtk_button_new_with_label("Send");
    gtk_box_pack_start(GTK_BOX(hbox), send_button, FALSE, FALSE, 0);

    char *group_name_copy = g_strdup(group_name);
    GtkWidget **widgets = g_malloc(sizeof(GtkWidget *) * 3);
    widgets[0] = message_entry;
    widgets[1] = chat_text_view;
    widgets[2] = (GtkWidget *)group_name_copy;

    g_signal_connect(send_button, "clicked", G_CALLBACK(send_group_message_gtk), widgets);

    gtk_widget_show_all(window);
}

void on_user_chat_selected(GtkWidget *widget __attribute__((unused)), gpointer user_data)
{
    const char *username = (const char *)user_data;
    open_chat_window(username);
    g_free(user_data);
}

void on_group_chat_selected(GtkWidget *widget __attribute__((unused)), gpointer group_data)
{
    const char *group_name = (const char *)group_data;
    open_group_chat_window(group_name);
    g_free(group_data);
}

void show_user_list(GtkWidget *widget __attribute__((unused)), gpointer data __attribute__((unused)))
{
    char response[1024];
    send(sock, "LIST_USERS", strlen("LIST_USERS"), 0);
    recv(sock, response, sizeof(response) - 1, 0);
    response[strcspn(response, "\n")] = '\0';

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "User List");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), box);

    char *user = strtok(response, ":");
    while (user)
    {
        if (strcmp(user, current_username) != 0)
        { // Bỏ qua username hiện tại
            GtkWidget *button = gtk_button_new_with_label(user);
            gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
            g_signal_connect(button, "clicked", G_CALLBACK(on_user_chat_selected), g_strdup(user));
        }
        user = strtok(NULL, ":");
    }

    gtk_widget_show_all(window);
}

void show_group_list(GtkWidget *widget __attribute__((unused)), gpointer data __attribute__((unused)))
{
    char response[1024];
    send(sock, "LIST_GROUPS", strlen("LIST_GROUPS"), 0);
    recv(sock, response, sizeof(response) - 1, 0);
    response[strcspn(response, "\n")] = '\0';

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Group List");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), box);

    char *group = strtok(response, ":");
    while (group)
    {
        GtkWidget *button = gtk_button_new_with_label(group);
        gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
        g_signal_connect(button, "clicked", G_CALLBACK(on_group_chat_selected), g_strdup(group));
        group = strtok(NULL, ":");
    }

    gtk_widget_show_all(window);
}

void on_register_button_clicked(GtkWidget *widget __attribute__((unused)), gpointer data __attribute__((unused)))
{
    const char *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(password_entry));
    const char *email = gtk_entry_get_text(GTK_ENTRY(email_entry));

    if (strlen(username) == 0 || strlen(password) == 0 || strlen(email) == 0)
    {
        g_print("Register failed: Username, password, or email cannot be empty.\n");
        return;
    }

    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "REGISTER %s %s %s", username, password, email);
    send(sock, buffer, strlen(buffer), 0);

    char response[1024];
    ssize_t received = recv(sock, response, sizeof(response) - 1, 0);
    if (received <= 0)
    {
        g_print("Failed to receive response from server.\n");
        return;
    }

    response[received] = '\0';
    response[strcspn(response, "\n")] = '\0';

    if (strcmp(response, "RegisterSuccess") == 0)
    {
        g_print("Register successful.\n");
    }
    else
    {
        g_print("Register failed: %s\n", response);
    }
}

void on_login_button_clicked(GtkWidget *widget __attribute__((unused)), gpointer data __attribute__((unused)))
{
    const char *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(password_entry));

    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "LOGIN %s %s", username, password);
    send(sock, buffer, strlen(buffer), 0);

    char response[1024];
    recv(sock, response, sizeof(response) - 1, 0);
    response[strcspn(response, "\n")] = '\0';

    if (strcmp(response, "LoginSuccess") == 0)
    {
        g_print("Login successful.\n");
        gtk_widget_destroy(main_window);
        strncpy(current_username, username, sizeof(current_username) - 1); // Lưu username hiện tại
        current_username[sizeof(current_username) - 1] = '\0';
        create_main_menu();
    }
    else
    {
        g_print("Login failed: %s\n", response);
    }
}

void on_file_sharing_button_clicked(GtkWidget *widget __attribute__((unused)), gpointer data __attribute__((unused)))
{
    g_print("File sharing button clicked.\n");
    search_files(sock, "*");
}

void create_main_menu()
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Main Menu");

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), box);

    GtkWidget *chat_button = gtk_button_new_with_label("Chat 1-1");
    GtkWidget *group_chat_button = gtk_button_new_with_label("Group Chat");
    GtkWidget *file_sharing_button = gtk_button_new_with_label("File Sharing");

    g_signal_connect(chat_button, "clicked", G_CALLBACK(show_user_list), NULL);
    g_signal_connect(group_chat_button, "clicked", G_CALLBACK(show_group_list), NULL);
    g_signal_connect(file_sharing_button, "clicked", G_CALLBACK(on_file_sharing_button_clicked), NULL);

    gtk_box_pack_start(GTK_BOX(box), chat_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), group_chat_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), file_sharing_button, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
}

void create_login_window()
{
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "Login/Register");

    GtkWidget *grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(main_window), grid);

    GtkWidget *username_label = gtk_label_new("Username:");
    username_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), username_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), username_entry, 1, 0, 1, 1);

    GtkWidget *password_label = gtk_label_new("Password:");
    password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_grid_attach(GTK_GRID(grid), password_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), password_entry, 1, 1, 1, 1);

    GtkWidget *email_label = gtk_label_new("Email:");
    email_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), email_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), email_entry, 1, 2, 1, 1);

    GtkWidget *login_button = gtk_button_new_with_label("Login");
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_button_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), login_button, 0, 3, 1, 1);

    GtkWidget *register_button = gtk_button_new_with_label("Register");
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_button_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), register_button, 1, 3, 1, 1);

    gtk_widget_show_all(main_window);
}

void *receive_chat_messages(void *arg)
{
    char buffer[BUFFER_SIZE];

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0);
        printf("%s\n", buffer);
        printf("hi\n");

        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';

            // if (strncmp(buffer, "CHAT_HISTORY:", 13) == 0)
            // {
            //     // Xử lý lịch sử trò chuyện
            //     const char *chat_history = buffer + 13;
            //     gtk_text_buffer_set_text(chat_buffer, chat_history, -1);
            // }
            // else
            // {
            // Phân loại tin nhắn và hiển thị
            if (strncmp(buffer, "MESSAGE:", 8) == 0)
            {
                const char message[4048];
                strcpy(message, buffer + 9);
                update_chat_display(message, 0); // Hiển thị tin nhắn từ người khác
            }
            else
            {
                update_chat_display(buffer, 1); // Hiển thị tin nhắn của chính mình
            }
            // }
        }
        else if (bytes_read == 0)
        {
            // Kết nối bị đóng
            printf("Connection closed by server.\n");
            break;
        }
        else
        {
            // Xử lý lỗi nhận dữ liệu
            perror("Error receiving message");
            break;
        }
    }
    return NULL;
}

void update_chat_display(const char *message, int is_self)
{
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(chat_buffer, &end);

    if (is_self)
    {
        gtk_text_buffer_insert(chat_buffer, &end, "\nMe: ", -1);
    }
    else
    {
        gtk_text_buffer_insert(chat_buffer, &end, "\n", -1);
    }
    gtk_text_buffer_insert(chat_buffer, &end, message, -1);
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    sock = connect_to_server("127.0.0.1", 8083);
    if (sock < 0)
    {
        g_print("Failed to connect to server.\n");
        return -1;
    }

    create_login_window();
    gtk_main();

    close(sock);
    return 0;
}
