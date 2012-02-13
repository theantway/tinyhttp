#ifndef http_H

#define MAX_REQUEST_LEN 2048

struct HttpRequest {
  int client_fd;
  char method[8];
  char version[16];
  char uri[MAX_REQUEST_LEN];
};

#endif
