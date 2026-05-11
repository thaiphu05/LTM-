#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

void signal_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

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

    signal(SIGCHLD, signal_handler);

    printf("Time Server is listening on port 8080...\n");

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client == -1) continue;

        if (fork() == 0) {
            close(listener);
            char buf[256], cmd[32], format[32], res[64];
            
            while (1) {
                int len = recv(client, buf, sizeof(buf) - 1, 0);
                if (len <= 0) break;
                
                buf[len] = 0;
                if (buf[len - 1] == '\n') buf[len - 1] = 0;
                if (buf[len - 1] == '\r') buf[len - 1] = 0;

                if (sscanf(buf, "%s %s", cmd, format) == 2) {
                    if (strcmp(cmd, "GET_TIME") == 0) {
                        time_t now = time(NULL);
                        struct tm *t = localtime(&now);
                        int valid = 1;

                        if (strcmp(format, "dd/mm/yyyy") == 0) 
                            strftime(res, sizeof(res), "%d/%m/%Y\n", t);
                        else if (strcmp(format, "dd/mm/yy") == 0)
                            strftime(res, sizeof(res), "%d/%m/%y\n", t);
                        else if (strcmp(format, "mm/dd/yyyy") == 0)
                            strftime(res, sizeof(res), "%m/%d/%Y\n", t);
                        else if (strcmp(format, "mm/dd/yy") == 0)
                            strftime(res, sizeof(res), "%m/%d/%y\n", t);
                        else {
                            valid = 0;
                            char *err = "Sai dinh dang thoi gian.\n";
                            send(client, err, strlen(err), 0);
                        }

                        if (valid) send(client, res, strlen(res), 0);
                    } else {
                        char *err = "Sai lenh. Dung GET_TIME [format]\n";
                        send(client, err, strlen(err), 0);
                    }
                } else {
                    char *err = "Thieu tham so.\n";
                    send(client, err, strlen(err), 0);
                }
            }
            close(client);
            exit(0);
        }
        close(client);
    }
    close(listener);
    return 0;
}