#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>

int main(int argc, char* argv[]){
  struct sockaddr_in server_addr;
  struct sockaddr remote_addr;

  int SERVER_PORT=10000;
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&server_addr, 0, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(SERVER_PORT);

  printf("binding on port %d\n", SERVER_PORT);
  bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

  if(listen(listen_fd, 1) < 0){
    printf("Could not listen on port\n");
    return -1;
  }

  while(1){
    socklen_t sock_len = sizeof(remote_addr);
    int client_fd = accept(listen_fd, &remote_addr, &sock_len);

    static char* result = "Welcome to network programming\n";
    write(client_fd, result, strlen(result));
    close(client_fd);
  }
  return 0;
}
