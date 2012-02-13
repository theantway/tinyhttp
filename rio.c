#include <errno.h>
#include <stdio.h>
#include <memory.h>

#include "rio.h"

void rio_init(rio_t* rp, int fd){
  rp->fd = fd;
  rp->bufptr = rp->buf;
  rp->unread_count = 0;
}

int rio_read(rio_t *rp, char* buf, size_t n){
  int count=n;

  while(rp->unread_count <= 0){
    rp->unread_count = read(rp->fd, rp->buf, sizeof(rp->buf));
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
