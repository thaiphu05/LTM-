#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define INPUT_BUF_SISSSSZE 4096

static volatile sig_atomic_t g_keep_running = 1;

static void handle_sigint(int sig) {
  (void)sig;
  g_keep_running = 0;
}

static int send_all(int sockfd, const char *data, size_t len) {
  size_t sent_total = 0;

  while (sent_total < len) {
    ssize_t sent = send(sockfd, data + sent_total, len - sent_total, 0);
    if (sent <= 0) {
      return -1;
    }
    sent_total += (size_t)sent;
  }

  return 0;
}

static int read_payload_from_stdin(int packet_index, int total_packets,
                                   char *buffer, size_t buffer_size,
                                   size_t *out_len) {
  while (1) {
    printf("Nhap goi tin %d/%d: ", packet_index, total_packets);
    fflush(stdout);

    if (fgets(buffer, (int)buffer_size, stdin) == NULL) {
      return -1;
    }

    size_t len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
      buffer[len - 1] = '\0';
      len--;
    }

    if (len == 0) {
      printf("Goi tin rong, vui long nhap lai.\n");
      continue;
    }

    *out_len = len;
    return 0;
  }
}

static void compute_payload_sums(const char *payload, size_t payload_len,
                                 unsigned long long *ascii_sum,
                                 unsigned long long *digit_sum,
                                 unsigned long long *integer_sum) {
  unsigned long long a_sum = 0;
  unsigned long long d_sum = 0;
  unsigned long long i_sum = 0;

  size_t i = 0;
  while (i < payload_len) {
    unsigned char ch = (unsigned char)payload[i];
    a_sum += ch;

    if (isdigit(ch)) {
      d_sum += (unsigned long long)(ch - '0');

      unsigned long long current = 0;
      while (i < payload_len && isdigit((unsigned char)payload[i])) {
        current = current * 10ULL + (unsigned long long)(payload[i] - '0');
        i++;
      }
      i_sum += current;
      continue;
    }

    i++;
  }

  *ascii_sum = a_sum;
  *digit_sum = d_sum;
  *integer_sum = i_sum;
}

static void sleep_ms(int ms) {
  if (ms <= 0) {
    return;
  }

  struct timespec req;
  req.tv_sec = ms / 1000;
  req.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&req, NULL);
}

int main(int argc, char *argv[]) {
  if (argc != 5) {
    fprintf(stderr,
            "Usage: streaming_client <INTERVAL_MS> <PACKET_COUNT> <IP_ADDRESS> <PORT>\n");
    fprintf(stderr,
            "  Vi du: streaming_client 500 4 127.0.0.1 8081\n");
    return 1;
  }

  int interval_ms = atoi(argv[1]);
  int packet_count = atoi(argv[2]);
  const char *ip_des = argv[3];
  int port = atoi(argv[4]);

  if (interval_ms < 0 || packet_count <= 0 || port <= 0) {
    fprintf(stderr, "Invalid interval, packet_count, or port\n");
    return 1;
  }

  int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (client == -1) {
    perror("socket() failed");
    return 1;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons((uint16_t)port);

  if (inet_pton(AF_INET, ip_des, &server_addr.sin_addr) <= 0) {
    perror("Invalid address");
    close(client);
    return 1;
  }

  if (connect(client, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("connect() failed");
    close(client);
    return 1;
  }

  signal(SIGINT, handle_sigint);

    printf("Connected to %s:%d | interval=%d ms | packet_count=%d\n", ip_des,
      port, interval_ms, packet_count);
    printf("Nhap %d goi tin (moi goi 1 dong).\n", packet_count);

  unsigned long long total_bytes_sent = 0;
  unsigned long long total_ascii_sum = 0;
  unsigned long long total_digit_sum = 0;
  unsigned long long total_integer_sum = 0;

  char payload[INPUT_BUF_SIZE];

  for (int i = 1; i <= packet_count && g_keep_running; i++) {
    size_t payload_len = 0;
    if (read_payload_from_stdin(i, packet_count, payload, sizeof(payload),
                                &payload_len) != 0) {
      fprintf(stderr, "Failed to read payload from stdin\n");
      break;
    }

    unsigned long long ascii_sum = 0;
    unsigned long long digit_sum = 0;
    unsigned long long integer_sum = 0;
    compute_payload_sums(payload, payload_len, &ascii_sum, &digit_sum,
                         &integer_sum);

    if (send_all(client, payload, payload_len) != 0) {
      perror("send() failed");
      break;
    }

    total_bytes_sent += (unsigned long long)payload_len;
    total_ascii_sum += ascii_sum;
    total_digit_sum += digit_sum;
    total_integer_sum += integer_sum;

        printf("Da gui goi %d/%d | bytes=%zu | ascii_sum=%llu | digit_sum=%llu | integer_sum=%llu\n",
          i, packet_count, payload_len, ascii_sum, digit_sum, integer_sum);

        if (i < packet_count) {
      sleep_ms(interval_ms);
    }
  }

  printf("Tong ket: bytes_sent=%llu | ascii_sum=%llu | digit_sum=%llu | integer_sum=%llu\n",
         total_bytes_sent, total_ascii_sum, total_digit_sum, total_integer_sum);

  close(client);
  return 0;
}
