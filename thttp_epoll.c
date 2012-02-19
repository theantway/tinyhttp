#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>

#include <sys/epoll.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "rio.h"
#include "http.h"

#define MAX_EVENTS 10


int http_parse_request(int fd, struct HttpRequest* request);
int http_handle_request(struct HttpRequest* request);

int main(int argc, char* argv[]){
  struct sockaddr_in server_addr;
  struct sockaddr_in remote_addr;
  struct epoll_event ev, events[MAX_EVENTS];

  const int BUFFER_SIZE = 4096;
  char buf[BUFFER_SIZE];

  int SERVER_PORT=10000;
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  int reuseaddr = 1;
  int epoll_fd = epoll_create(0);
  if(epoll_fd == -1){
    printf("could not create epoll descriptor\n");
    return -1;
  }

  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
  memset(&server_addr, 0, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(SERVER_PORT);

  printf("binding on port %d\n", SERVER_PORT);
  if(bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
    printf("Could not bind address\n");
    return -2;
  }

  if(listen(listen_fd, 1) < 0){
    printf("Could not listen on port\n");
    return -3;
  }

  ev.events = EPOLLIN;
  ev.data.fd = listen_fd;
  
  if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1){
    printf("Could not add listen fd to epoll descriptor\n");
    return -4;
  }

  while(1){
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if(nfds == -1){
      printf("Could not wait epoll events\n");
      return -5;
    }

    for(int n = 0; n < nfds; n++){
      if(events[n].data.fd == listen_fd){
	socklen_t sock_len = sizeof(remote_addr);
	int client_fd = accept(listen_fd, (struct sockaddr*)&remote_addr, &sock_len);
	if(client_fd == -1){
	  printf("Could not accpet connection request\n");
	}

	char client_ip[32];
	inet_ntop(AF_INET, (const void *)&remote_addr.sin_addr, client_ip, sizeof(client_ip));
	printf("Request from %s[fd:%d]\n", client_ip, client_fd);

	setnonblocking(client_fd);
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = client_fd;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1){
	  printf("Could not add new connection request to epoll descriptor\n");
	}
      }else{
	int client_fd = events[n].data.fd;
	if(events[n].events & EPOLLIN){
	  buffered_request_t* buffer = buffer_read_all_available_data(client_fd);
	  //TODO: handle request only if already a full request

	  struct HttpRequest request;
	  http_parse_request(buffer, &request);
	  http_handle_request(&request);
	}

	if(events[n].events & EPOLLOUT){
	  buffer_write_all_available_data(client_fd);
	}

	if(events[n].events & EPOLLHUP){
	  buffer_clear(client_fd);
	  close(client_fd);
	}
      }
    }
  }
  return 0;
}

#define REQUEST_MAX_LENGTH 8192

#define CONNECTION_BUFFER_SIZE 100
#define WRITE_BUFFER_SIZE 8192

struct buffered_request {
  rio_t* rio;

  int readpos;
  int read_length;
  char readbuf[REQUEST_MAX_LENGTH];

  int writepos;
  char writebuf[WRITE_BUFFER_SIZE];

  buffered_request_t *next;
} buffered_request_t;

static buffered_request_t* buffered_requests[CONNECTION_BUFFER_SIZE];

/*
TODO: need to handle huge request which length more than REQUEST_MAX_LENGTH
*/
buffered_request_t* buffer_read_all_available_data(int fd){
  buffered_request_t* r = buffered_request_for_connection(fd);
  int total_size = 0;
  while(1){
    int len = buffer_read(r);
    if(len == -1){
      return r;
    }
    total_size += len;
  }
}

int buffer_read(buffered_request_t* r){
  int read_count = rio_read(r->rio, r->buf + r->readpos, sizeof(r->buf) - r->readpos);
  if(read_count == -1){
    return read_count;
  }

  r->readpos += read_count;
  return read_count;
}

static buffered_request_for_connection(int fd){
  
  buffered_request_t* r = buffered_requests[fd];
  while(r != NULL){
    if(r->rio->fd == fd){
      return r;
    }

    r = r->next;
  }

  return NULL;
}

#define MAX_LINE 8194

int http_parse_request(buffered_request_t* buffer, struct HttpRequest* request){
  char buf[MAX_LINE];
  int line_length;

  request->client_fd = buffer->rio->fd;
  
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
