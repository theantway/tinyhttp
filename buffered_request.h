#ifndef BUFFERED_REQUEST_H
#define BUFFERED_REQUEST_H

#include "rio.h"

#define REQUEST_MAX_LENGTH 8192

#define CONNECTION_BUFFER_SIZE 200
#define WRITE_BUFFER_SIZE 8192
#define MAX_LINE 8194

#include "ev.h"

typedef struct buffered_request {
    rio_t *rio;
  //  #if USING_LIBEV
    ev_io request_ev;
  //#endif
    int readpos;
    int unread_length;
    char readbuf[REQUEST_MAX_LENGTH];

    int writepos;
    int unwrite_length;
    char writebuf[WRITE_BUFFER_SIZE];

    struct buffered_request *previous;
    struct buffered_request *next;
} buffered_request_t;

buffered_request_t *buffered_request_init(int fd);

void buffered_request_clear(buffered_request_t * r);

/*
TODO: need to handle huge request which length more than REQUEST_MAX_LENGTH
*/
int buffered_request_read_all_available_data(buffered_request_t * r);

int buffered_request_write_all_available_data(buffered_request_t * r);

int buffered_request_add_response(buffered_request_t* r, char* buf, int length);

int buffered_request_has_wroten_all(buffered_request_t* r);

buffered_request_t *buffered_request_for_connection(int fd);

int buffered_request_readline(buffered_request_t * buffered, char *buf, int max_length);

#endif
