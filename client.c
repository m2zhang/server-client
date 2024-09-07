#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "record.h"
#include <stdlib.h>
#include <signal.h>

#define MY_PORT 21213

void handle_sigpipe(int sig) {
    fprintf(stderr, "SIGPIPE caught\n");
    exit(1);
}

// server address in dot notation, server port number
int main(int argc, char **argv)
{
  if (argc < 3) {
    fprintf(stderr, "Missing server address or server port number.\n");
    return 1;
  }

  int cfd;
  struct sockaddr_in a;

  memset(&a, 0, sizeof(struct sockaddr_in));
  a.sin_family = AF_INET;
  a.sin_port = htons(atoi(argv[2])); // port number
  if (0 == inet_pton(AF_INET, argv[1], &a.sin_addr)) { // server address
    fprintf(stderr, "That's not an IPv4 address.\n");
    return 1;
  }

  cfd = socket(AF_INET, SOCK_STREAM, 0);
  
  if (cfd == -1) {
        perror("socket");
        return 1;
    }

  if (-1 == connect(cfd, (struct sockaddr *)&a, sizeof(struct sockaddr_in))) {
    perror("connect");
    return 1;
  } else {
    printf("Ready\n");
  }

  // Handle SIGPIPE
  signal(SIGPIPE, handle_sigpipe);

  // fgets.. 30.. supposedly 30 chars is 30 bytes...?
  for (;;) {

    char buf[31]; // read from stdin
    char rec[11]; // get the # of spots from server

    // accounts for EOF and blank line
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
          close(cfd); // EOF/error reading from stdin
          exit(0);
    }
    if ((strlen(buf) == 1 && buf[0] == '\n')){ // we got just empty lines
        close(cfd);
        exit(0);
    }

    if (write(cfd, buf, strlen(buf)) == -1) {
        perror("write");
        close(cfd);
        exit(1);
        // alr handled SIGPIPE by setting the signal earlier
    }

    size_t totalBytesRead = 0;
    size_t bytesRequested = 11;
    int foundNewline = 0;

    while (totalBytesRead < bytesRequested) {
      ssize_t bytesRead = read(cfd, rec+ totalBytesRead, bytesRequested - totalBytesRead);
    
      if (bytesRead == -1) {
          perror("Error reading from server"); // perror or fprintf?
          close(cfd);
          exit(1);
          break;
      } else if (bytesRead == 0) { // accounted for errors/EOF
          //perror("Unexpected disconnection!?");
          //close(cfd);
          //exit(1);
          fprintf(stderr, "Connection closed by server\n");
          break;
      }

      totalBytesRead += bytesRead;

      // Check for newline character in the newly read bytes
      for (size_t i = totalBytesRead - bytesRead; i < totalBytesRead; i++) {
        if (rec[i] == '\n') {
            foundNewline = 1;
            break;
        }
      }

      if (foundNewline ==1){
        break;
      }

    }

    if (totalBytesRead >= 11 && !foundNewline){
        fprintf(stderr, "Server bug\n");
        close(cfd);
        exit(1);
      }  
    
    printf("%.*s", (int)totalBytesRead, rec);

  }

    
  // close(cfd);
  // I don't think we need... cause it'll be closed in the for loop somewhere..

  return 0;
}
