#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 1024

static int append_chunk(char **buf, size_t *used, size_t *capacity,
                        const char *chunk, size_t chunk_len) {
  size_t required = *used + chunk_len + 1;
  if (required > *capacity) {
    size_t new_capacity = *capacity;
    while (new_capacity < required) {
      new_capacity *= 2;
    }

    char *new_buf = realloc(*buf, new_capacity);
    if (new_buf == NULL) {
      return -1;
    }

    *buf = new_buf;
    *capacity = new_capacity;
  }

  memcpy(*buf + *used, chunk, chunk_len);
  *used += chunk_len;
  (*buf)[*used] = '\0';
  return 0;
}

static void parse_and_log_payload(const char *payload, FILE *log_file,
                                  const char *client_ip) {
  char *copy = strdup(payload);
  if (copy == NULL) {
    fprintf(stderr, "Memory allocation failed while parsing payload\n");
    return;
  }

  char workdir[BUF_SIZE] = "";
  int listed_files = 0;
  long long total_bytes = 0;
  int declared_total_files = -1;

  char *saveptr = NULL;
  char *line = strtok_r(copy, "\n", &saveptr);

  while (line != NULL) {
    if (strncmp(line, "WORKDIR:", 8) == 0) {
      snprintf(workdir, sizeof(workdir), "%s", line + 8);
    } else if (strncmp(line, "TOTAL_FILES:", 12) == 0) {
      declared_total_files = atoi(line + 12);
    } else if (strcmp(line, "FILES:") != 0 && line[0] != '\0') {
      char filename[BUF_SIZE];
      long long size = 0;

      if (sscanf(line, "%[^|]|%lld", filename, &size) == 2) {
        listed_files++;
        total_bytes += size;

        printf("  File: %s (%lld bytes)\n", filename, size);
        fprintf(log_file, "CLIENT=%s WORKDIR=%s FILE=%s SIZE=%lld\n", client_ip,
                workdir[0] ? workdir : "N/A", filename, size);
      }
    }

    line = strtok_r(NULL, "\n", &saveptr);
  }

  printf("Received package from %s | workdir=%s | files=%d | total_bytes=%lld\n",
         client_ip, workdir[0] ? workdir : "N/A", listed_files, total_bytes);

  fprintf(log_file,
          "SUMMARY CLIENT=%s WORKDIR=%s LISTED_FILES=%d DECLARED_FILES=%d "
          "TOTAL_BYTES=%lld\n",
          client_ip, workdir[0] ? workdir : "N/A", listed_files,
          declared_total_files, total_bytes);
  fflush(log_file);

  free(copy);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: infor_server <PORT> <PATH_LOG>\n");
    exit(1);
  }

  int port = atoi(argv[1]);
  const char *path_log = argv[2];

  FILE *log_file = fopen(path_log, "a");
  if (log_file == NULL) {
    perror("Failed to open log file");
    exit(1);
  }

  int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == -1) {
    perror("socket() failed");
    exit(1);
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind() failed");
    fclose(log_file);
    close(listener);
    exit(1);
  }

  if (listen(listener, 5) < 0) {
    perror("listen() failed");
    fclose(log_file);
    close(listener);
    exit(1);
  }

  printf("Server is listening on port %d\n", port);

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

    size_t capacity = 4096;
    size_t used = 0;
    char *payload = malloc(capacity);
    if (payload == NULL) {
      fprintf(stderr, "Memory allocation failed: %s\n", strerror(errno));
      close(client);
      continue;
    }
    payload[0] = '\0';

    char buffer[BUF_SIZE];
    while (1) {
      ssize_t received = recv(client, buffer, sizeof(buffer), 0);
      if (received < 0) {
        perror("recv() failed");
        break;
      }

      if (received == 0) {
        break;
      }

      if (append_chunk(&payload, &used, &capacity, buffer, (size_t)received) !=
          0) {
        fprintf(stderr, "Failed to grow receive buffer\n");
        break;
      }
    }

    if (used > 0) {
      parse_and_log_payload(payload, log_file, client_ip);
    } else {
      printf("Client %s connected but sent no data\n", client_ip);
    }

    free(payload);
    close(client);

    if (used == 0) {
      break;
    }
  }

  fclose(log_file);
  close(listener);
  return 0;
}