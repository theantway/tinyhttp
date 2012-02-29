#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "ev.h"
#include "rio.h"
#include "http.h"

#define USING_LIBEV 1

#define MAX_EVENTS 200

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

void write_cb(struct ev_loop *loop, ev_io *request_evio, int revents){
  buffered_request_t *buffered = buffered_request_for_connection(request_evio->fd);

  if(!buffered_request_has_wroten_all(buffered)){
    static int enable_cork = 1;
    setsockopt(request_evio->fd, SOL_TCP, TCP_CORK, &enable_cork, sizeof(enable_cork));
    //printf("[%d]SEND response:\n%s\n", client_fd, &(buffered->writebuf[buffered->writepos]));
    buffered_request_write_all_available_data(buffered);
                                         
    if(buffered_request_has_wroten_all(buffered)){
      //printf("[%d]Disconnected\n", client_fd);
      ev_io_stop(EV_A_ request_evio);
      http_close_connection(buffered);
    }
  }
}

void read_cb(struct ev_loop *loop, ev_io *request_evio, int revents){
  buffered_request_t *buffered = buffered_request_for_connection(request_evio->fd);
               
  int read_count = buffered_request_read_all_available_data(buffered);
  if(read_count > 0){
    //printf("[%d]Received request:\n%s\n", client_fd, &(buffered->readbuf[buffered->readpos]));
                        
    //TODO: handle request only if already a full request
    http_request_t request;
    if(http_parse_request(buffered, &request)< 0){
      printf("failed to parse request\n");
    }
                    
    http_handle_request(buffered, &request);
    ev_io_stop(EV_A_ request_evio);
    ev_io_init(&buffered->request_ev, write_cb, request_evio->fd, EV_WRITE);
    ev_io_start(loop, &buffered->request_ev);
  }else if(read_count == 0){
    printf("Close connection because read 0\n");
    ev_io_stop(EV_A_ request_evio);
    http_close_connection(buffered);
  }

}

void accept_cb(struct ev_loop *loop, ev_io *listen_evio, int revents){
  struct sockaddr_in remote_addr;
  socklen_t sock_len = sizeof(remote_addr);
  int client_fd = accept(listen_evio->fd, (struct sockaddr *)&remote_addr, &sock_len);

  if (client_fd == -1) {
    printf("Could not accept connection request\n");
  }

  //char client_ip[32];
  //inet_ntop(AF_INET, (const void *)&remote_addr.sin_addr, client_ip, sizeof(client_ip));
  //printf("Request from %s[fd:%d]\n", client_ip, client_fd);

  buffered_request_t* request = buffered_request_init(client_fd);

  setnonblocking(client_fd);
  ev_io_init(&request->request_ev, read_cb, client_fd, EV_READ);

  ev_io_start(loop, &request->request_ev);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    
    //    struct epoll_event ev, events[MAX_EVENTS];
    ev_io ev_accept;
    struct ev_loop *loop = EV_DEFAULT;
    
    const int BUFFER_SIZE = 4096;
    char buf[BUFFER_SIZE];

    int SERVER_PORT = 10000;
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int reuseaddr = 1;

    //    bzero(&ev, sizeof(ev));
    
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

    ev_io_init (&ev_accept, accept_cb, listen_fd, EV_READ);
    ev_io_start (loop, &ev_accept);

    ev_run(loop, 0);
    
    return 0;
}
