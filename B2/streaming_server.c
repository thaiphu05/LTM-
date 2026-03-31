#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define RECV_BUF_SIZE 1024
#define TARGET "0123456789"

static unsigned long long count_target_in_buffer(const char *buffer,
                                                 size_t buffer_len,
                                                 const char *target,
                                                 size_t target_len) {
  if (buffer_len < target_len) {
    return 0;
  }

  unsigned long long count = 0;
  for (size_t i = 0; i + target_len <= buffer_len; i++) {
    if (memcmp(buffer + i, target, target_len) == 0) {
      count++;
    }
  }
  return count;
}

int main(int argc, char *argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: streaming_server <PORT> [PATH_LOG]\n");
    return 1;
  }

  int port = atoi(argv[1]);
  if (port <= 0) {
    fprintf(stderr, "Invalid port\n");
    return 1;
  }

  FILE *log_file = NULL;
  if (argc == 3) {
    log_file = fopen(argv[2], "a");
    if (log_file == NULL) {
      fprintf(stderr, "Cannot open log file '%s': %s\n", argv[2],
              strerror(errno));
      return 1;
    }
  }

  int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == -1) {
    perror("socket() failed");
    if (log_file != NULL) {
      fclose(log_file);
    }
    return 1;
  }

  int reuse = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons((uint16_t)port);

  if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind() failed");
    close(listener);
    if (log_file != NULL) {
      fclose(log_file);
    }
    return 1;
  }

  if (listen(listener, 5) < 0) {
    perror("listen() failed");
    close(listener);
    if (log_file != NULL) {
      fclose(log_file);
    }
    return 1;
  }

  const char *target = TARGET;
  size_t target_len = strlen(target);

  printf("Streaming server is listening on port %d\n", port);
  printf("Target string: %s\n", target);

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client = accept(listener, (struct sockaddr *)&client_addr, &client_len);
    if (client < 0) {
      perror("accept() failed");
      continue;
    }

    char client_ip[INET_ADDRSTRLEN] = "unknown";
    if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)) ==
        NULL) {
      snprintf(client_ip, sizeof(client_ip), "unknown");
    }

    printf("Client connected: %s:%d\n", client_ip,
           ntohs(client_addr.sin_port));

    unsigned long long total_count = 0;
    unsigned long long recv_count = 0;
    unsigned long long total_bytes = 0;

    char carry[64];
    size_t carry_len = 0;
    memset(carry, 0, sizeof(carry));

    char recv_buf[RECV_BUF_SIZE];

    while (1) {
      ssize_t received = recv(client, recv_buf, sizeof(recv_buf), 0);
      if (received < 0) {
        perror("recv() failed");
        break;
      }

      if (received == 0) {
        printf("Client disconnected: %s\n", client_ip);
        break;
      }

      recv_count++;
      total_bytes += (unsigned long long)received;

      size_t merged_len = carry_len + (size_t)received;
      char *merged = malloc(merged_len);
      if (merged == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        break;
      }

      memcpy(merged, carry, carry_len);
      memcpy(merged + carry_len, recv_buf, (size_t)received);

      unsigned long long added =
          count_target_in_buffer(merged, merged_len, target, target_len);
      total_count += added;

      size_t next_carry_len = 0;
      if (merged_len >= target_len - 1) {
        next_carry_len = target_len - 1;
      } else {
        next_carry_len = merged_len;
      }

      if (next_carry_len > sizeof(carry)) {
        next_carry_len = sizeof(carry);
      }

      memcpy(carry, merged + (merged_len - next_carry_len), next_carry_len);
      carry_len = next_carry_len;
      free(merged);

      printf("recv#%llu bytes=%zd | new=%llu | total=%llu\n", recv_count,
             received, added, total_count);

      if (log_file != NULL) {
        fprintf(log_file,
                "CLIENT=%s RECV=%llu BYTES=%zd NEW_MATCH=%llu TOTAL=%llu\n",
                client_ip, recv_count, received, added, total_count);
        fflush(log_file);
      }
    }

    close(client);
  }

  close(listener);
  if (log_file != NULL) {
    fclose(log_file);
  }
  return 0;
}
