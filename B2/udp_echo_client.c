#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 2048

static void trim_newline(char *s) {
  size_t len = strlen(s);
  while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
    s[len - 1] = '\0';
    len--;
  }
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: udp_echo_client <IP_ADDRESS> <PORT>\n");
    return 1;
  }

  const char *server_ip = argv[1];
  int port = atoi(argv[2]);

  if (port <= 0 || port > 65535) {
    fprintf(stderr, "Invalid port\n");
    return 1;
  }

  int client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (client < 0) {
    perror("socket() failed");
    return 1;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons((uint16_t)port);

  if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
    perror("Invalid address");
    close(client);
    return 1;
  }

  printf("Connected to UDP echo server %s:%d\n", server_ip, port);
  printf("Nhap tin nhan, go /quit de thoat.\n");

  char send_buf[BUF_SIZE];
  char recv_buf[BUF_SIZE];

  while (1) {
    printf("> ");
    fflush(stdout);

    if (fgets(send_buf, sizeof(send_buf), stdin) == NULL) {
      break;
    }

    trim_newline(send_buf);

    if (strcmp(send_buf, "/quit") == 0) {
      break;
    }

    size_t msg_len = strlen(send_buf);
    if (msg_len == 0) {
      continue;
    }

    ssize_t sent = sendto(client, send_buf, msg_len, 0,
                          (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (sent < 0) {
      perror("sendto() failed");
      break;
    }

    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    ssize_t received = recvfrom(client, recv_buf, sizeof(recv_buf) - 1, 0,
                                (struct sockaddr *)&from_addr, &from_len);
    if (received < 0) {
      perror("recvfrom() failed");
      break;
    }

    recv_buf[received] = '\0';
    printf("Echo: %s\n", recv_buf);
  }

  close(client);
  return 0;
}
