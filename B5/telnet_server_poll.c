#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#define MAX_CLIENTS 1000

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

    struct pollfd fds[MAX_CLIENTS];
    int logged[MAX_CLIENTS];
    
    fds[0].fd = listener;
    fds[0].events = POLLIN;
    int nfds = 1;

    char buf[256], cmd[512], user[64], pass[64], line[128];

    while (1) {
        int ret = poll(fds, nfds, -1);
        if (ret < 0) break;

        if (fds[0].revents & POLLIN) {
            int client = accept(listener, NULL, NULL);
            if (nfds < MAX_CLIENTS) {
                fds[nfds].fd = client;
                fds[nfds].events = POLLIN;
                logged[nfds] = 0;
                nfds++;
                send(client, "Nhap user pass: ", 16, 0);
            } else {
                close(client);
            }
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                ret = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);
                if (ret <= 0) {
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    logged[i] = logged[nfds - 1];
                    nfds--;
                    i--;
                } else {
                    buf[ret] = 0;
                    if (buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = 0;
                    if (buf[strlen(buf) - 1] == '\r') buf[strlen(buf) - 1] = 0;

                    if (logged[i] == 0) {
                        if (sscanf(buf, "%s %s", user, pass) == 2) {
                            FILE *f = fopen("db.txt", "r");
                            int found = 0;
                            if (f) {
                                while (fgets(line, sizeof(line), f)) {
                                    char u[64], p[64];
                                    if (sscanf(line, "%s %s", u, p) == 2) {
                                        if (strcmp(user, u) == 0 && strcmp(pass, p) == 0) {
                                            found = 1; break;
                                        }
                                    }
                                }
                                fclose(f);
                            }
                            if (found) {
                                logged[i] = 1;
                                send(fds[i].fd, "Success. Nhap lenh: ", 20, 0);
                            } else {
                                send(fds[i].fd, "Sai. Nhap lai: ", 15, 0);
                            }
                        }
                    } else {
                        sprintf(cmd, "%s > out.txt", buf);
                        system(cmd);
                        FILE *f = fopen("out.txt", "r");
                        if (f) {
                            char res[1024];
                            while ((ret = fread(res, 1, sizeof(res), f)) > 0) {
                                send(fds[i].fd, res, ret, 0);
                            }
                            fclose(f);
                        }
                        send(fds[i].fd, "\n>", 2, 0);
                    }
                }
            }
        }
    }
    close(listener);
    return 0;
}