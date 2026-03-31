#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 8080
#define MAX_BUFFER 1024

typedef struct {
    int step; 
    char name[MAX_BUFFER];
    char mssv[MAX_BUFFER];
} ClientState;

void generate_email(char* full_name, char* mssv, char* email_out) {
    char name_copy[MAX_BUFFER];
    strncpy(name_copy, full_name, MAX_BUFFER);

    for(int i = 0; name_copy[i]; i++) {
        name_copy[i] = tolower(name_copy[i]);
    }

    char *parts[20];
    int count = 0;
    char *token = strtok(name_copy, " ");
    while(token != NULL && count < 20) {
        parts[count++] = token;
        token = strtok(NULL, " ");
    }

    if(count == 0) {
        strcpy(email_out, "invalid@sis.hust.edu.vn");
        return;
    }

    char ho_dem[50] = "";
    for(int i = 0; i < count - 1; i++) {
        strncat(ho_dem, parts[i], 1);
    }
    
    char *ten = parts[count - 1];

    char *mssv_short = mssv;
    int len = strlen(mssv);
    if(len > 6) {
        mssv_short = mssv + (len - 6);
    }

    sprintf(email_out, "%s.%s%s@sis.hust.edu.vn", ten, ho_dem, mssv_short);
}

void trim_newline(char *str) {
    str[strcspn(str, "\r\n")] = 0;
}

int main() {
    int listener, newfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen;
    
    char buffer[MAX_BUFFER];
    
    fd_set master_fds; 
    fd_set read_fds;   
    int fdmax;         

    ClientState clients[FD_SETSIZE];
    memset(clients, 0, sizeof(clients));

    FD_ZERO(&master_fds);
    FD_ZERO(&read_fds);

    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        exit(1);
    }

    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    memset(&(server_addr.sin_zero), '\0', 8);

    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind error");
        exit(1);
    }

    if (listen(listener, 10) == -1) {
        perror("listen error");
        exit(1);
    }

    FD_SET(listener, &master_fds);
    fdmax = listener;

    printf("[*] Non-blocking Server (C) đang lắng nghe tại port %d...\n", PORT);

    while(1) {
        read_fds = master_fds; 

        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select error");
            exit(1);
        }

        for(int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                
                if (i == listener) {
                    addrlen = sizeof(client_addr);
                    newfd = accept(listener, (struct sockaddr *)&client_addr, &addrlen);

                    if (newfd == -1) {
                        perror("accept error");
                    } else {
                        FD_SET(newfd, &master_fds);
                        if (newfd > fdmax) {
                            fdmax = newfd;
                        }
                        printf("[+] Client mới kết nối từ %s trên socket %d\n", inet_ntoa(client_addr.sin_addr), newfd);
                        
                        clients[newfd].step = 1;
                        char *msg = "Server: Ho ten: ";
                        send(newfd, msg, strlen(msg), 0);
                    }
                } else {
                    int nbytes = recv(i, buffer, sizeof(buffer) - 1, 0);

                    if (nbytes <= 0) {
                        if (nbytes == 0) {
                            printf("[-] Socket %d da ngat ket noi\n", i);
                        } else {
                            perror("recv error");
                        }
                        close(i);
                        FD_CLR(i, &master_fds);
                        clients[i].step = 0; 
                    } else {
                        buffer[nbytes] = '\0';
                        trim_newline(buffer);

                        if (clients[i].step == 1) {
                            strncpy(clients[i].name, buffer, MAX_BUFFER);
                            clients[i].step = 2;
                            char *msg = "Server: MSSV: ";
                            send(i, msg, strlen(msg), 0);
                        } 
                        else if (clients[i].step == 2) {
                            strncpy(clients[i].mssv, buffer, MAX_BUFFER);
                            
                            char email_result[MAX_BUFFER];
                            generate_email(clients[i].name, clients[i].mssv, email_result);
                            
                            char response[MAX_BUFFER + 50];
                            sprintf(response, "Server: mailhust : %s\n", email_result);
                            send(i, response, strlen(response), 0);

                            close(i);
                            FD_CLR(i, &master_fds);
                            clients[i].step = 0;
                            printf("[-] Đã phục vụ xong và đóng kết nối socket %d\n", i);
                        }
                    }
                }
            }
        }
    }
    return 0;
}