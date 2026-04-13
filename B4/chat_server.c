#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
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

    char names[FD_SETSIZE][64];
    for (int i = 0; i < FD_SETSIZE; i++) names[i][0] = 0;

    char buf[256], out[512];

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
                        char *msg = "Nhap ten: client_id: name\n";
                        send(client, msg, strlen(msg), 0);
                    } else {
                        close(client);
                    }
                } else {
                    ret = recv(i, buf, sizeof(buf) - 1, 0);
                    if (ret <= 0) {
                        FD_CLR(i, &fdread);
                        close(i);
                        names[i][0] = 0;
                    } else {
                        buf[ret] = 0;
                        if (buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = 0;
                        if (buf[strlen(buf) - 1] == '\r') buf[strlen(buf) - 1] = 0;

                        if (names[i][0] == 0) {
                            if (strncmp(buf, "client_id: ", 11) == 0) {
                                strcpy(names[i], buf + 11);
                                send(i, "Success\n", 8, 0);
                            } else {
                                char *msg = "Sai cu phap\n";
                                send(i, msg, strlen(msg), 0);
                            }
                        } else {
                            time_t now = time(NULL);
                            struct tm *t = localtime(&now);
                            char time_s[32];
                            strftime(time_s, sizeof(time_s), "%Y/%m/%d %I:%M:%S%p", t);

                            sprintf(out, "%s %s: %s\n", time_s, names[i], buf);
                            for (int j = 0; j < FD_SETSIZE; j++) {
                                if (FD_ISSET(j, &fdread) && j != listener && j != i && names[j][0] != 0) {
                                    send(j, out, strlen(out), 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    close(listener);
    return 0;
}