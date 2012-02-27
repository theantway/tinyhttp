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

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    struct sockaddr_in remote_addr;
    struct epoll_event ev, events[MAX_EVENTS];

    const int BUFFER_SIZE = 4096;
    char buf[BUFFER_SIZE];

    int SERVER_PORT = 10000;
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int reuseaddr = 1;

    bzero(&ev, sizeof(ev));
    int epoll_fd = epoll_create(200/*deprecated?*/);
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

                //char client_ip[32];
                //inet_ntop(AF_INET, (const void *)&remote_addr.sin_addr, client_ip, sizeof(client_ip));
                //printf("Request from %s[fd:%d]\n", client_ip, client_fd);

                buffered_request_init(client_fd);

                setnonblocking(client_fd);
                ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    printf("Could not add new connection request to epoll descriptor\n");
                }
            } else {
                int client_fd = events[n].data.fd;
                buffered_request_t *buffered = buffered_request_for_connection(client_fd);

                if (events[n].events & EPOLLHUP) {
                  printf("[%d]Hup Disconnected\n", client_fd);
                    http_close_connection(buffered);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, &ev);                    
                    continue;
                }
                
                if (events[n].events & EPOLLIN) {
                    int read_count = buffered_request_read_all_available_data(buffered);
                    if(read_count > 0){
                      //printf("[%d]Received request:\n%s\n", client_fd, &(buffered->readbuf[buffered->readpos]));
                        
                        //TODO: handle request only if already a full request
                        http_request_t request;
                        if(http_parse_request(buffered, &request)< 0){
                          printf("failed to parse request\n");
                        }
                    
                        http_handle_request(buffered, &request);
                    }else if(read_count == 0){
                      http_close_connection(buffered);
                      epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, &ev);                      
                      continue;
                    }
                }

                if (events[n].events & EPOLLOUT) {
                  if(!buffered_request_has_wroten_all(buffered)){
                    //printf("[%d]SEND response:\n%s\n", client_fd, &(buffered->writebuf[buffered->writepos]));
                    buffered_request_write_all_available_data(buffered);
                                         
                    if(buffered_request_has_wroten_all(buffered)){
                      //printf("[%d]Disconnected\n", client_fd);
                      http_close_connection(buffered);
                      epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, &ev);
                      continue;
                      }
                    
                  }
                }

                if (events[n].events & EPOLLERR) {
                    printf("[%d] ERR Disconnected\n", client_fd);

                    http_close_connection(buffered);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, &ev);                    
                    continue;
                }

            }
        }
    }
    return 0;
}
