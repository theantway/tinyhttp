#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_REQUEST_LEN 2048

struct HttpRequest {
  int client_fd;
  char method[8];
  char version[8];
  char uri[MAX_REQUEST_LEN];
};

struct HttpRequest* http_parse_request(int fd);
int http_handle_request(struct HttpRequest* request);

extern int errno;

int main(int argc, char* argv[]){
  struct sockaddr_in server_addr;
  struct sockaddr remote_addr;

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
    int client_fd = accept(listen_fd, &remote_addr, &sock_len);

    static char* result = "Welcome to network programming\n";

    struct HttpRequest* request = http_parse_request(client_fd);
    http_handle_request(request);
    
    close(client_fd);
  }
  return 0;
}

#define RIO_BUFFERSIZE 8192

typedef struct {
  int fd;
  int unread_count;
  char buf[RIO_BUFFERSIZE];
  char* bufptr;
} rio_t;

void rio_init(rio_t* rp, int fd){
  rp->fd = fd;
  rp->bufptr = rp->buf;
  rp->unread_count = 0;
}

int rio_read(rio_t *rp, char* buf, size_t n){
  int count=n;

  while(rp->unread_count <= 0){
    rp->unread_count = read(rp->fd, rp->buf, sizeof(rp->buf));
    printf("\nDEBUG:%s", rp->buf);
    if(rp->unread_count < 0){
      if(errno != EINTR){
	return -1;
      }
    }else if(rp->unread_count ==0){
      return EOF;
    }else{
      rp->bufptr = rp->buf;
    }
  }

  if(rp->unread_count < n){
    count = rp->unread_count;
  }

  memcpy(buf, rp->bufptr, count);
  rp->bufptr +=count;
  rp->unread_count -=count;

  return count;
}

ssize_t rio_readline(rio_t* rp, void* buf, size_t maxlen){
  char c, *bufp=buf;
  int n;

  for(n = 0; n < maxlen; ++n){
    int len = rio_read(rp, &c, 1);

    if(len > 0){
      *bufp++ = c;
    }else if(len ==0){
      if(n > 0){
	break;
      }else{
	return 0;
      }
    }else{
      return -1;
    }

    if(c == '\n'){
      break;
    }    
  }

  *bufp='\0';
  return n;
}

#define MAX_LINE 8194

struct HttpRequest* http_parse_request(int fd){
  char buf[MAX_LINE], method[MAX_LINE], uri[MAX_LINE], version[MAX_LINE];
  int line_length;

  struct HttpRequest* request = malloc(sizeof(struct HttpRequest));
  request->client_fd = fd;
  
  rio_t rio;
  rio_init(&rio, fd);

  //TODO: need to check result, try to telnet and input nothing
  line_length = rio_readline(&rio, buf, MAX_LINE);
  printf("get first line(%d): %s\n", line_length, buf);
  sscanf(buf, "%s %s %s", method, uri, version);

  strncpy(request->method, method, sizeof(request->method));
  strncpy(request->uri, uri, sizeof(request->uri));
  strncpy(request->version, version, sizeof(request->version));

  while((line_length = rio_readline(&rio, buf, MAX_LINE)) > 0){
    printf("new line: %s", buf);
    if(strncmp(buf, "\r\n", line_length) == 0){
      break;
    }
    if(strncmp(buf, "\n", line_length) == 0){
      printf("got unexpected new line\n");
      break;
    }
  }
  /*
  strncpy(request->uri, "/", sizeof(request->uri));
  strncpy(request->method, "GET", sizeof(request->method));
  strncpy(request->version, "HTTP/1.0", sizeof(request->version));
  */

  return request;
}

int http_handle_request(struct HttpRequest* request){
  char message[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nWelcome to network programming!\n";

  printf("Response to: %s %s %s\n", request->method, request->uri, request->version);
  write(request->client_fd, message, strlen(message));

  return 0;
}
