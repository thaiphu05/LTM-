#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) return 1;

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) return 1;
    if (listen(listener, 10)) return 1;

    fd_set fdread, fdtest;
    FD_ZERO(&fdread);
    FD_SET(listener, &fdread);

    int logged_in[FD_SETSIZE];
    for (int i = 0; i < FD_SETSIZE; i++) logged_in[i] = 0;

    char buf[256], user[64], pass[64], line[128], cmd[512];

    while (1) {
        fdtest = fdread;
        int ret = select(FD_SETSIZE, &fdtest, NULL, NULL, NULL);
        if (ret < 0) break;

        for (int i = 0; i < FD_SETSIZE; i++) {
            if (FD_ISSET(i, &fdtest)) {
                if (i == listener) {
                    int client = accept(listener, NULL, NULL);
                    if (client < FD_SETSIZE) {
                        FD_SET(client, &fdread);
                        char *msg = "Nhap user pass: ";
                        send(client, msg, strlen(msg), 0);
                    } else {
                        close(client);
                    }
                } else {
                    ret = recv(i, buf, sizeof(buf) - 1, 0);
                    if (ret <= 0) {
                        FD_CLR(i, &fdread);
                        close(i);
                        logged_in[i] = 0;
                    } else {
                        buf[ret] = 0;
                        if (buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = 0;
                        if (buf[strlen(buf) - 1] == '\r') buf[strlen(buf) - 1] = 0;

                        if (logged_in[i] == 0) {
                            if (sscanf(buf, "%s %s", user, pass) == 2) {
                                FILE *f = fopen("db.txt", "r");
                                int found = 0;
                                if (f != NULL) {
                                    while (fgets(line, sizeof(line), f)) {
                                        char db_user[64], db_pass[64];
                                        if (sscanf(line, "%s %s", db_user, db_pass) == 2) {
                                            if (strcmp(user, db_user) == 0 && strcmp(pass, db_pass) == 0) {
                                                found = 1;
                                                break;
                                            }
                                        }
                                    }
                                    fclose(f);
                                }
                                if (found) {
                                    logged_in[i] = 1;
                                    send(i, "Login success. Nhap lenh: ", 26, 0);
                                } else {
                                    send(i, "Sai tai khoan. Nhap lai: ", 25, 0);
                                }
                            } else {
                                send(i, "Nhap dung dinh dang 'user pass': ", 33, 0);
                            }
                        } else {
                            sprintf(cmd, "%s > out.txt", buf);
                            system(cmd);
                            
                            FILE *f = fopen("out.txt", "r");
                            if (f != NULL) {
                                char result[1024];
                                while (ret = fread(result, 1, sizeof(result), f)) {
                                    send(i, result, ret, 0);
                                }
                                fclose(f);
                            }
                            send(i, "\nNhap lenh tiep: ", 17, 0);
                        }
                    }
                }
            }
        }
    }

    close(listener);
    return 0;
}