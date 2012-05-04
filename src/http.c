#include "http.h"
#include <stdio.h>
#include <string.h>

void http_close_connection(buffered_request_t* buffered){
  close(buffered->rio->fd);
  buffered_request_clear(buffered);

}

int http_parse_request(buffered_request_t * buffer, http_request_t *request) {
    char* buf = NULL;
    int line_length;

    request->client_fd = buffer->rio->fd;

    //printf("Parse request\n========\n");
    //TODO: need to check result, try to telnet and input nothing
    if(buffered_request_readline(buffer, buf, MAX_LINE) <= 0){
      printf("failed to read line\n");
      return -1;
    }

    //printf("%s", buf);
    /*
    char format[16];

    sprintf(format, "%%%lus %%%lus %%%lus", sizeof(request->method) - 1, sizeof(request->uri) - 1,
        sizeof(request->version) - 1);
    sscanf(buf, format, request->method, request->uri, request->version);
    */
    strcpy(request->uri, "/");
    while ((line_length = buffered_request_readline(buffer, buf, MAX_LINE)) > 0) {
      //        printf(" %s", buf);
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

int http_handle_request(buffered_request_t * buffered, http_request_t *request) {
    char message[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 32\r\n\r\nWelcome to network programming!\n";

    char message_404[] = "HTTP/1.1 404 NOT FOUND\r\nContent-Length: 0\r\n\r\n";
    
    //printf("Response to: %s %s %s\n\n", request->method, request->uri, request->version);

    if(strcmp(request->uri, "/favicon.ico") == 0){
      buffered_request_add_response(buffered, message_404, strlen(message_404));
    }else{
      buffered_request_add_response(buffered, message, strlen(message));
    }
    return 0;
}
