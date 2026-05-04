#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <port> <remote_ip> <remote_port>\n", argv[0]);
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        perror("socket() failed");
        return 1;
    }

    struct sockaddr_in local_addr = {0};
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(atoi(argv[1]));

    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr))) {
        perror("bind() failed");
        close(sock);
        return 1;
    }

    struct sockaddr_in remote_addr = {0};
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = inet_addr(argv[2]);
    remote_addr.sin_port = htons(atoi(argv[3]));

    fd_set fdread;
    char buf[1024];

    while (1) {
        FD_ZERO(&fdread);
        FD_SET(STDIN_FILENO, &fdread);
        FD_SET(sock, &fdread);

        int ret = select(sock + 1, &fdread, NULL, NULL, NULL);
        if (ret < 0) {
            perror("select() failed");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &fdread)) {
            if (fgets(buf, sizeof(buf), stdin) == NULL) {
                break;
            }
            sendto(sock, buf, strlen(buf), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
        }

        if (FD_ISSET(sock, &fdread)) {
            struct sockaddr_in sender_addr = {0};
            socklen_t sender_len = sizeof(sender_addr);
            
            ret = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&sender_addr, &sender_len);
            if (ret > 0) {
                buf[ret] = 0;
                printf("[%s:%d]: %s", inet_ntoa(sender_addr.sin_addr), ntohs(sender_addr.sin_port), buf);
            }
        }
    }

    close(sock);
    return 0;
}