#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

struct Client {
    int fd;
    char topics[10][64];
    int num_topics;
};

int main() {
    int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == -1) {
        perror("socket() failed");
        return 1;
    }

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9000);

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(server);
        return 1;
    }

    if (listen(server, 10)) {
        perror("listen() failed");
        close(server);
        return 1;
    }

    struct Client clients[100] = {0};
    fd_set fdread;
    char buf[1024];

    while (1) {
        FD_ZERO(&fdread);
        FD_SET(server, &fdread);
        int max_fd = server;

        for (int i = 0; i < 100; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &fdread);
                if (clients[i].fd > max_fd) {
                    max_fd = clients[i].fd;
                }
            }
        }

        int ret = select(max_fd + 1, &fdread, NULL, NULL, NULL);
        if (ret < 0) {
            perror("select() failed");
            break;
        }

        if (FD_ISSET(server, &fdread)) {
            int client_fd = accept(server, NULL, NULL);
            for (int i = 0; i < 100; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = client_fd;
                    clients[i].num_topics = 0;
                    break;
                }
            }
        }

        for (int i = 0; i < 100; i++) {
            int c_fd = clients[i].fd;
            if (c_fd > 0 && FD_ISSET(c_fd, &fdread)) {
                ret = recv(c_fd, buf, sizeof(buf) - 1, 0);
                if (ret <= 0) {
                    close(c_fd);
                    clients[i].fd = 0;
                    clients[i].num_topics = 0;
                } else {
                    buf[ret] = 0;
                    
                    while (ret > 0 && (buf[ret - 1] == '\r' || buf[ret - 1] == '\n')) {
                        buf[--ret] = 0;
                    }

                    if (strncmp(buf, "SUB ", 4) == 0) {
                        char topic[64] = {0};
                        sscanf(buf + 4, "%63s", topic);
                        if (strlen(topic) > 0 && clients[i].num_topics < 10) {
                            int exists = 0;
                            for (int j = 0; j < clients[i].num_topics; j++) {
                                if (strcmp(clients[i].topics[j], topic) == 0) {
                                    exists = 1; 
                                    break;
                                }
                            }
                            if (!exists) {
                                strcpy(clients[i].topics[clients[i].num_topics++], topic);
                            }
                        }
                    } else if (strncmp(buf, "PUB ", 4) == 0) {
                        char topic[64] = {0};
                        sscanf(buf + 4, "%63s", topic);
                        int topic_len = strlen(topic);
                        if (topic_len > 0) {
                            char *msg_ptr = buf + 4 + topic_len;
                            while (*msg_ptr == ' ') msg_ptr++;
                            
                            char send_buf[1024];
                            sprintf(send_buf, "[%s]: %s\n", topic, msg_ptr);
                            
                            for (int j = 0; j < 100; j++) {
                                if (clients[j].fd > 0) {
                                    for (int k = 0; k < clients[j].num_topics; k++) {
                                        if (strcmp(clients[j].topics[k], topic) == 0) {
                                            send(clients[j].fd, send_buf, strlen(send_buf), 0);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    close(server);
    return 0;
}