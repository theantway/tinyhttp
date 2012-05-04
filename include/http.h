#ifndef HTTP_H
#define HTTP_H

#include "buffered_request.h"

#define MAX_REQUEST_LEN 2048

typedef struct http_request {
  int client_fd;
  char method[8];
  char version[16];
  char uri[MAX_REQUEST_LEN];
} http_request_t;

void http_close_connection(buffered_request_t* buffered);
int http_parse_request(buffered_request_t * buffer, http_request_t *request);
int http_handle_request(buffered_request_t * buffered, http_request_t *request);

#endif
