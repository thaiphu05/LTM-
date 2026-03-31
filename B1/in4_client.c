#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 1024

static int send_all(int sockfd, const void *data, size_t len) {
  const char *p = (const char *)data;
  size_t sent_total = 0;

  while (sent_total < len) {
    ssize_t sent = send(sockfd, p + sent_total, len - sent_total, 0);
    if (sent <= 0) {
      return -1;
    }
    sent_total += (size_t)sent;
  }

  return 0;
}

static const char *get_dir_name(const char *path) {
  const char *last_slash = strrchr(path, '/');
  if (last_slash == NULL) {
    return path;
  }

  if (*(last_slash + 1) == '\0') {
    static char tmp[PATH_MAX];
    size_t len = strlen(path);

    while (len > 0 && path[len - 1] == '/') {
      len--;
    }

    if (len == 0) {
      return "/";
    }

    memcpy(tmp, path, len);
    tmp[len] = '\0';

    last_slash = strrchr(tmp, '/');
    return (last_slash == NULL) ? tmp : (last_slash + 1);
  }

  return last_slash + 1;
}

static int append_text(char **buf, size_t *used, size_t *capacity,
                       const char *text) {
  size_t add_len = strlen(text);
  size_t required = *used + add_len + 1;

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

  memcpy(*buf + *used, text, add_len);
  *used += add_len;
  (*buf)[*used] = '\0';
  return 0;
}

static int build_package(const char *workdir, char **out_payload,
                         size_t *out_len) {
  DIR *dir = opendir(workdir);
  if (dir == NULL) {
    return -1;
  }

  size_t capacity = 4096;
  size_t used = 0;
  int file_count = 0;
  char *payload = malloc(capacity);
  if (payload == NULL) {
    closedir(dir);
    return -1;
  }
  payload[0] = '\0';

  char header[BUF_SIZE];
  snprintf(header, sizeof(header), "WORKDIR:%s\n", get_dir_name(workdir));
  if (append_text(&payload, &used, &capacity, header) != 0) {
    free(payload);
    closedir(dir);
    return -1;
  }

  if (append_text(&payload, &used, &capacity, "FILES:\n") != 0) {
    free(payload);
    closedir(dir);
    return -1;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char full_path[PATH_MAX];
    struct stat st;

    snprintf(full_path, sizeof(full_path), "%s/%s", workdir, entry->d_name);
    if (stat(full_path, &st) != 0) {
      continue;
    }

    if (!S_ISREG(st.st_mode)) {
      continue;
    }

    char line[BUF_SIZE];
    snprintf(line, sizeof(line), "%s|%lld\n", entry->d_name,
             (long long)st.st_size);

    if (append_text(&payload, &used, &capacity, line) != 0) {
      free(payload);
      closedir(dir);
      return -1;
    }

    file_count++;
  }

  char summary[BUF_SIZE];
  snprintf(summary, sizeof(summary), "TOTAL_FILES:%d\n", file_count);
  if (append_text(&payload, &used, &capacity, summary) != 0) {
    free(payload);
    closedir(dir);
    return -1;
  }

  closedir(dir);
  *out_payload = payload;
  *out_len = used;
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Usage: infor_client <IP_ADDRESS> <PORT> <WORKDIR>\n");
    exit(1);
  }infor

  const char *ip_des = argv[1];
  int port = atoi(argv[2]);
  const char *workdir = argv[3];

  int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (client == -1) {
    perror("socket() failed");
    exit(1);
  }

  struct sockaddr_in server_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
  };

  if (inet_pton(AF_INET, ip_des, &server_addr.sin_addr) <= 0) {
    perror("Invalid address");
    exit(1);
  }

  if (connect(client, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("connect() failed");
    exit(1);
  }

  printf("Connected to server %s:%d\n", ip_des, port);

  char *payload = NULL;
  size_t payload_len = 0;

  if (build_package(workdir, &payload, &payload_len) != 0) {
    fprintf(stderr, "Failed to build package from workdir '%s': %s\n", workdir,
            strerror(errno));
    close(client);
    return 1;
  }

  if (send_all(client, payload, payload_len) != 0) {
    perror("send() failed");
    free(payload);
    close(client);
    return 1;
  }

  printf("Sent package (%zu bytes) from workdir '%s'\n", payload_len, workdir);

  free(payload);

  close(client);
  return 0;
}