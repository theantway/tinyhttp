#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rio.h"
#include "http.h"

int http_parse_request(int fd, struct HttpRequest* request);
int http_handle_request(struct HttpRequest* request);

int main(int argc, char* argv[]){
  struct sockaddr_in server_addr;
  struct sockaddr_in remote_addr;

  const int BUFFER_SIZE = 4096;
  char buf[BUFFER_SIZE];

  int SERVER_PORT=10000;
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  int reuseaddr = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
  memset(&server_addr, 0, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(SERVER_PORT);

  printf("binding on port %d\n", SERVER_PORT);
  if(bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
    printf("Could not bind address\n");
    return -1;
  }

  if(listen(listen_fd, 1) < 0){
    printf("Could not listen on port\n");
    return -2;
  }

  while(1){
    socklen_t sock_len = sizeof(remote_addr);
    int client_fd = accept(listen_fd, (struct sockaddr*)&remote_addr, &sock_len);

    char client_ip[32];
    inet_ntop(AF_INET, (const void *)&remote_addr.sin_addr, client_ip, sizeof(client_ip));

    printf("Request from %s[fd:%d]\n", client_ip, client_fd);
    struct HttpRequest request;
    http_parse_request(client_fd, &request);
    http_handle_request(&request);
    
    close(client_fd);
  }
  return 0;
}

#define MAX_LINE 8194

int http_parse_request(int fd, struct HttpRequest* request){
  char buf[MAX_LINE];
  int line_length;

  request->client_fd = fd;
  
  rio_t rio;
  rio_init(&rio, fd);

  //TODO: need to check result, try to telnet and input nothing
  line_length = rio_readline(&rio, buf, MAX_LINE);

  printf("%s", buf);
  
  char format[16];
  sprintf(format, "%%%lus %%%lus %%%lus", sizeof(request->method) - 1, sizeof(request->uri) - 1, sizeof(request->version) - 1);
  sscanf(buf, format, request->method, request->uri, request->version);

  while((line_length = rio_readline(&rio, buf, MAX_LINE)) > 0){
    printf(" %s", buf);
    if(strncmp(buf, "\r\n", line_length) == 0){
      break;
    }
    if(strncmp(buf, "\n", line_length) == 0){
      printf("got unexpected new line\n");
      break;
    }
  }

  return 0;
}

int http_handle_request(struct HttpRequest* request){
  char message[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nWelcome to network programming!\n";

  printf("Response to: %s %s %s\n\n", request->method, request->uri, request->version);
  write(request->client_fd, message, strlen(message));

  return 0;
}
