#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>

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
    char *names[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) names[i] = NULL;

    fds[0].fd = listener;
    fds[0].events = POLLIN;
    int nfds = 1;

    char buf[256], out[512];

    while (1) {
        int ret = poll(fds, nfds, -1);
        if (ret < 0) break;

        if (fds[0].revents & POLLIN) {
            int client = accept(listener, NULL, NULL);
            if (nfds < MAX_CLIENTS) {
                fds[nfds].fd = client;
                fds[nfds].events = POLLIN;
                names[nfds] = NULL;
                nfds++;
                char *msg = "Nhap ten: client_id: name\n";
                send(client, msg, strlen(msg), 0);
            } else {
                close(client);
            }
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                ret = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);
                if (ret <= 0) {
                    close(fds[i].fd);
                    if (names[i]) free(names[i]);
                    fds[i] = fds[nfds - 1];
                    names[i] = names[nfds - 1];
                    nfds--;
                    i--;
                } else {
                    buf[ret] = 0;
                    if (buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = 0;
                    if (buf[strlen(buf) - 1] == '\r') buf[strlen(buf) - 1] = 0;

                    if (names[i] == NULL) {
                        if (strncmp(buf, "client_id: ", 11) == 0) {
                            names[i] = strdup(buf + 11);
                            send(fds[i].fd, "OK\n", 3, 0);
                        } else {
                            char *msg = "Sai cu phap\n";
                            send(fds[i].fd, msg, strlen(msg), 0);
                        }
                    } else {
                        time_t now = time(NULL);
                        struct tm *t = localtime(&now);
                        char time_s[32];
                        strftime(time_s, sizeof(time_s), "%Y/%m/%d %I:%M:%S%p", t);

                        sprintf(out, "%s %s: %s\n", time_s, names[i], buf);
                        for (int j = 1; j < nfds; j++) {
                            if (j != i && names[j] != NULL) {
                                send(fds[j].fd, out, strlen(out), 0);
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