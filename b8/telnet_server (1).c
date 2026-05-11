#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 9000

int check_auth(const char *user, const char *pass) {
    FILE *f = fopen("users.txt", "r");
    if (!f) return 0;

    char line[256];
    char f_user[128], f_pass[128];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%127s %127s", f_user, f_pass) == 2) {
            if (strcmp(user, f_user) == 0 && strcmp(pass, f_pass) == 0) {
                fclose(f);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
}

void handle_client(int client_fd) {
    char buf[1024];
    int logged_in = 0;
    pid_t pid = getpid();

    char *msg = "Vui long dang nhap (user pass):\n> ";
    send(client_fd, msg, strlen(msg), 0);

    while (1) {
        memset(buf, 0, sizeof(buf));
        int bytes_received = recv(client_fd, buf, sizeof(buf) - 1, 0);

        if (bytes_received <= 0) break;

        buf[strcspn(buf, "\r\n")] = 0;
        if (strlen(buf) == 0) {
            char *prompt = logged_in ? "> " : "Vui long dang nhap (user pass):\n> ";
            send(client_fd, prompt, strlen(prompt), 0);
            continue;
        }

        if (!logged_in) {
            char user[128], pass[128];
            if (sscanf(buf, "%127s %127s", user, pass) == 2) {
                if (check_auth(user, pass)) {
                    logged_in = 1;
                    send(client_fd, "Thanh cong!\n> ", 13, 0);
                } else {
                    send(client_fd, "Sai account!\n> ", 15, 0);
                }
            }
        } else {
            // 1. Tạo file tạm riêng cho tiến trình này để lấy kết quả lệnh vừa nhập
            char temp_file[64];
            sprintf(temp_file, "temp_%d.txt", pid);

            // 2. Thực thi lệnh và lưu vào file tạm
            char cmd[2048];
            sprintf(cmd, "%s > %s 2>&1", buf, temp_file);
            system(cmd);

            // 3. Đọc file tạm gửi cho client và đồng thời ghi nối vào output.txt
            FILE *f_temp = fopen(temp_file, "r");
            if (f_temp) {
                // Mở file output.txt ở chế độ "a" (append - ghi tiếp vào cuối)
                FILE *f_out = fopen("output.txt", "a");
                if (f_out) {
                    fprintf(f_out, "----- Command: %s -----\n", buf); // Ghi chú tên lệnh vào log
                }

                char file_buf[1024];
                int bytes_read;
                while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), f_temp)) > 0) {
                    // Gửi cho client xem
                    send(client_fd, file_buf, bytes_read, 0);
                    // Ghi vào file output.txt chung
                    if (f_out) fwrite(file_buf, 1, bytes_read, f_out);
                }

                if (f_out) {
                    fprintf(f_out, "\n");
                    fclose(f_out);
                }
                fclose(f_temp);
                remove(temp_file); // Xoá file tạm, chỉ giữ lại output.txt
            }
            send(client_fd, "\n> ", 3, 0);
        }
    }
    close(client_fd);
    exit(0);
}

int main() {
    int listener, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);

    signal(SIGCHLD, SIG_IGN); // Tự động dọn dẹp tiến trình con

    listener = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(listener, 10);

    printf("Server dang chay... Ket qua se luu vao output.txt\n");

    while (1) {
        client_fd = accept(listener, (struct sockaddr *)&client_addr, &addrlen);
        if (fork() == 0) {
            close(listener);
            handle_client(client_fd);
        }
        close(client_fd);
    }
    return 0;
}
