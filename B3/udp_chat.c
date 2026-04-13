#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

int main(int argc, char *argv[]) {
    if (argc != 4) return 1;

    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    struct sockaddr_in s_addr = {0};
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_port = htons(atoi(argv[1]));

    if (bind(s, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0) return 1;

    struct sockaddr_in d_addr = {0};
    d_addr.sin_family = AF_INET;
    d_addr.sin_addr.s_addr = inet_addr(argv[2]);
    d_addr.sin_port = htons(atoi(argv[3]));

    fd_set fds;
    char b[1024];

    while (1) {
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(s, &fds);

        if (select(s + 1, &fds, NULL, NULL, NULL) < 0) break;

        if (FD_ISSET(0, &fds)) {
            if (fgets(b, sizeof(b), stdin) == NULL) break;
            sendto(s, b, strlen(b), 0, (struct sockaddr *)&d_addr, sizeof(d_addr));
        }

        if (FD_ISSET(s, &fds)) {
            int n = recvfrom(s, b, sizeof(b) - 1, 0, NULL, NULL);
            if (n > 0) {
                b[n] = 0;
                printf("Received: %s", b);
            }
        }
    }

    close(s);
    return 0;
}