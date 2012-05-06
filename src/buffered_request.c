#include "buffered_request.h"
#include <stdlib.h>
#include <string.h>

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

void buffered_request_add_response(buffered_request_t* r, char* buf, int length){
  int left_space = sizeof(r->writebuf) - r->writepos - r->unwrite_length;
  int len = length > left_space ? left_space : length;

  strncpy(&r->writebuf[r->writepos + r->unwrite_length], buf, len);
  
  r->unwrite_length += len;
  r->writebuf[r->writepos + r->unwrite_length + len] = '\0';
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
      // printf("empty slot\n");
        buffered_requests[fd] = request;
        return request;
    }

    int idx = 0;
    while (request_in_same_slot->next != NULL) {
        request_in_same_slot = request_in_same_slot->next;
        idx ++;
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

buffered_request_t *buffered_request_for_connection(int fd) {
    buffered_request_t *r = buffered_requests[fd];

    while (r != NULL) {
        if (r->rio->fd == fd) {
            return r;
        }

        r = r->next;
    }

    return NULL;
}

int buffered_request_readline(buffered_request_t * buffered, char *buf, int max_length) {
    int n;
    int unread_length = buffered->unread_length;
    int max = max_length - 1 > unread_length ? unread_length : max_length -1;
    
    char* source_bufp = &buffered->readbuf[buffered->readpos];
    buf = source_bufp;
    
    for (n = 0; n < max && ( *(source_bufp++) != '\n'); ++n) {
    }

    //    strncpy(buf, &buffered->readbuf[buffered->readpos], n);
    
    buffered->readpos +=n;
    buffered->unread_length -=n;
    //buf[n] = '\0';
    return n;
}
