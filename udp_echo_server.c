#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 2048

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: udp_echo_server <PORT>\n");
    return 1;
  }

  int port = atoi(argv[1]);
  if (port <= 0 || port > 65535) {
    fprintf(stderr, "Invalid port\n");
    return 1;
  }

  int server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (server < 0) {
    perror("socket() failed");
    return 1;
  }

  int reuse = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons((uint16_t)port);

  if (bind(server, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("bind() failed");
    close(server);
    return 1;
  }

  printf("UDP echo server listening on port %d\n", port);

  while (1) {
    char buffer[BUF_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    ssize_t received = recvfrom(server, buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr *)&client_addr, &client_len);
    if (received < 0) {
      perror("recvfrom() failed");
      continue;
    }

    buffer[received] = '\0';

    char client_ip[INET_ADDRSTRLEN] = "unknown";
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr.sin_port);

    printf("From %s:%d | %s\n", client_ip, client_port, buffer);

    ssize_t sent = sendto(server, buffer, (size_t)received, 0,
                          (struct sockaddr *)&client_addr, client_len);
    if (sent < 0) {
      perror("sendto() failed");
    }
  }

  close(server);
  return 0;
}
