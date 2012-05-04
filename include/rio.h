#ifndef RIO_H
#define RIO_H

#include <unistd.h>

#define RIO_BUFFERSIZE 8192

typedef struct {
  int fd;
  int unread_count;
  char buf[RIO_BUFFERSIZE];
  char* bufptr;
} rio_t;

void rio_init(rio_t* rp, int fd);
int rio_read(rio_t *rp, char* buf, size_t n);
int rio_write(rio_t *rp, const char* buf, size_t n);
ssize_t rio_readline(rio_t* rp, void* buf, size_t maxlen);

#endif
