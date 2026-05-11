#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

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

    printf("HTTP Server is listening on port 8080...\n");

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client == -1) continue;

        if (fork() == 0) {

            close(listener);
            
            char buf[1024];
            int ret = recv(client, buf, sizeof(buf) - 1, 0);
            if (ret > 0) {
                buf[ret] = 0;
                printf("Request received:\n%s\n", buf);

                char *msg = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Xin chao cac ban</h1></body></html>";
                send(client, msg, strlen(msg), 0);
            }

            close(client);
            exit(0);
        }
        
  
        close(client);
    }

    close(listener);
    return 0;
}