#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rio.h"
#include "http.h"

#define MAX_EVENTS 10

int setnonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | SOCK_NONBLOCK) < 0) {
        return -1;
    }

    return 0;
}

#define REQUEST_MAX_LENGTH 8192

#define CONNECTION_BUFFER_SIZE 100
#define WRITE_BUFFER_SIZE 8192

typedef struct buffered_request {
    rio_t *rio;

    int readpos;
    int unread_length;
    char readbuf[REQUEST_MAX_LENGTH];

    int writepos;
    int unwrite_length;
    char writebuf[WRITE_BUFFER_SIZE];

    struct buffered_request *previous;
    struct buffered_request *next;
} buffered_request_t;

static buffered_request_t *buffered_requests[CONNECTION_BUFFER_SIZE];

int buffered_request_read(buffered_request_t * r) {
    int read_count = rio_read(r->rio, r->readbuf + r->readpos, sizeof(r->readbuf) - r->readpos);

    if (read_count == -1) {
        return read_count;
    }

    r->unread_length += read_count;
    return read_count;
}

int buffered_request_write(buffered_request_t * r) {
    int write_count = rio_write(r->rio, r->writebuf + r->writepos, r->unwrite_length);

    if (write_count == -1) {
        return write_count;
    }

    r->writepos += write_count;
    r->unwrite_length -=write_count;
    return write_count;
}

int buffered_request_add_response(buffered_request_t* r, char* buf, int length){
  int n;
  int left_space = sizeof(r->writebuf) - r->writepos - r->unwrite_length;
  for(n = 0; n < length && n < left_space; n++){
    r->writebuf[r->writepos + r->unwrite_length + n] = buf[n];
  }

  r->unwrite_length += n;
  r->writebuf[r->writepos + r->unwrite_length + n] = '\0';

  return n;
}

buffered_request_t *buffered_request_init(int fd) {
    buffered_request_t *request = malloc(sizeof(buffered_request_t));
    request->rio = malloc(sizeof(rio_t));
    request->readpos = 0;
    request->writepos = 0;
    request->unread_length = 0;
    request->unwrite_length = 0;
    request->next = NULL;
    request->previous = NULL;
    
    rio_init(request->rio, fd);

    buffered_request_t *request_in_same_slot = buffered_requests[fd];
    if (request_in_same_slot == NULL) {
      printf("empty slot\n");
        buffered_requests[fd] = request;
        return request;
    }

    while (request_in_same_slot->next != NULL) {
        request_in_same_slot = request_in_same_slot->next;
    }

    request_in_same_slot->next = request;
    request->previous = request_in_same_slot;

    return request;
}

void buffered_request_clear(buffered_request_t * r) {
    buffered_request_t *requests_slot = buffered_requests[r->rio->fd];
    if (requests_slot == r) {
        buffered_requests[r->rio->fd] = r->next;
    } else {
        r->previous->next = r->next;
        r->next->previous = r->previous;
    }

    free(r->rio);
    free(r);
    r = NULL;
}

/*
TODO: need to handle huge request which length more than REQUEST_MAX_LENGTH
*/
int buffered_request_read_all_available_data(buffered_request_t * r) {
    int total_size = 0;

    while (1) {
        int len = buffered_request_read(r);

        if (len == -1) {
            return total_size;
        }
        total_size += len;
    }
}

int buffered_request_write_all_available_data(buffered_request_t * r) {
    buffered_request_write(r);

    return 1;
}

int buffered_request_has_wroten_all(buffered_request_t* r){
  if(r->unwrite_length > 0){
    return 0;
  }

  return 1;
}

static buffered_request_t *buffered_request_for_connection(int fd) {
    buffered_request_t *r = buffered_requests[fd];

    while (r != NULL) {
        if (r->rio->fd == fd) {
            return r;
        }

        r = r->next;
    }

    return NULL;
}

int http_parse_request(buffered_request_t * buffered, struct HttpRequest *request);
int http_handle_request(buffered_request_t * buffered, struct HttpRequest *request);

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    struct sockaddr_in remote_addr;
    struct epoll_event ev, events[MAX_EVENTS];

    const int BUFFER_SIZE = 4096;
    char buf[BUFFER_SIZE];

    int SERVER_PORT = 10000;
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int reuseaddr = 1;

    int epoll_fd = epoll_create(10/*deprecated?*/);
    if (epoll_fd == -1) {
        printf("could not create epoll descriptor\n");
        return -1;
    }
    
    setnonblocking(listen_fd);
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    printf("binding on port %d\n", SERVER_PORT);
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Could not bind address\n");
        return -2;
    }

    if (listen(listen_fd, 1) < 0) {
        printf("Could not listen on port\n");
        return -3;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        printf("Could not add listen fd to epoll descriptor\n");
        return -4;
    }

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            printf("Could not wait epoll events\n");
            return -5;
        }

        for (int n = 0; n < nfds; n++) {
            if (events[n].data.fd == listen_fd) {
                socklen_t sock_len = sizeof(remote_addr);
                int client_fd = accept(listen_fd, (struct sockaddr *)&remote_addr, &sock_len);
                if (client_fd == -1) {
                    printf("Could not accept connection request\n");
                }

                char client_ip[32];
                inet_ntop(AF_INET, (const void *)&remote_addr.sin_addr, client_ip, sizeof(client_ip));
                printf("Request from %s[fd:%d]\n", client_ip, client_fd);

                buffered_request_init(client_fd);

                setnonblocking(client_fd);
                ev.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLET;
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    printf("Could not add new connection request to epoll descriptor\n");
                }
            } else {
                int client_fd = events[n].data.fd;
                buffered_request_t *buffered = buffered_request_for_connection(client_fd);

                if (events[n].events & EPOLLIN) {
                    int read_count = buffered_request_read_all_available_data(buffered);
                    if(read_count > 0){
                      //printf("[%d]Received request:\n%s\n", client_fd, &(buffered->readbuf[buffered->readpos]));
                        
                        //TODO: handle request only if already a full request
                        struct HttpRequest request;
                        if(http_parse_request(buffered, &request)< 0){
                          printf("failed to parse request\n");
                        }
                    
                        http_handle_request(buffered, &request);
                    }else{
                      //printf("read 0 bytes\n");
                    }
                }

                if (events[n].events & EPOLLOUT) {
                  if(!buffered_request_has_wroten_all(buffered)){
                    //printf("[%d]SEND response:\n%s\n", client_fd, &(buffered->writebuf[buffered->writepos]));
                    buffered_request_write_all_available_data(buffered);
                    if(buffered_request_has_wroten_all(buffered)){
                      //printf("[%d]Disconnected\n", client_fd);
                      buffered_request_clear(buffered);
                      epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, &ev);
                      close(client_fd);
                    }
                  }
                }

                if (events[n].events & EPOLLHUP) {
                  //printf("[%d]Disconnected\n", client_fd);
                    buffered_request_clear(buffered);
                    close(client_fd);
                }
            }
        }
    }
    return 0;
}

#define MAX_LINE 8194

int buffered_request_readline(buffered_request_t * buffered, char *buf, int max_length) {
    int n;
    for (n = 0; n < max_length -1 && n < buffered->unread_length; n++) {
        buf[n] = buffered->readbuf[buffered->readpos + n];

        if (buf[n] == '\n') {
          n++;
          break;
        }
    }

    buffered->readpos +=n;
    buffered->unread_length -=n;
    buf[n] = '\0';
    return n;
}

int http_parse_request(buffered_request_t * buffer, struct HttpRequest *request) {
    char buf[MAX_LINE];
    int line_length;

    request->client_fd = buffer->rio->fd;

    //printf("Parse request\n========\n");
    //TODO: need to check result, try to telnet and input nothing
    if(buffered_request_readline(buffer, buf, MAX_LINE) <= 0){
      printf("failed to read line\n");
      return -1;
    }

    //printf("%s", buf);

    char format[16];

    sprintf(format, "%%%lus %%%lus %%%lus", sizeof(request->method) - 1, sizeof(request->uri) - 1,
        sizeof(request->version) - 1);
    sscanf(buf, format, request->method, request->uri, request->version);

    while ((line_length = buffered_request_readline(buffer, buf, MAX_LINE)) > 0) {
        printf(" %s", buf);
        if (strncmp(buf, "\r\n", line_length) == 0) {
            break;
        }
        if (strncmp(buf, "\n", line_length) == 0) {
            printf("got unexpected new line\n");
            break;
        }
    }

    return 0;
}

int http_handle_request(buffered_request_t * buffered, struct HttpRequest *request) {
    char message[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 32\r\n\r\nWelcome to network programming!\n";

    char message_404[] = "HTTP/1.1 404 NOT FOUND\r\n\r\n";
    
    //printf("Response to: %s %s %s\n\n", request->method, request->uri, request->version);

    if(strcmp(request->uri, "/favicon.ico") == 0){
      buffered_request_add_response(buffered, message_404, strlen(message_404));
    }else{
      buffered_request_add_response(buffered, message, strlen(message));
    }
    return 0;
}
