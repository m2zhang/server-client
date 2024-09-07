#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include "record.h"
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>  // for waitpid
#include <errno.h>

#define MY_PORT 21213

void ignore_sigpipe(void)
{
    struct sigaction myaction;

    myaction.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &myaction, NULL);
}

static int find_record(FILE *f, const char *name, record *pr)
{
    unsigned n = strlen(name);

    fseek(f, 0, SEEK_SET);

    while (fread(pr, sizeof(record), 1, f) == 1) {
        if (n == pr->name_len && strncmp(name, pr->name, n) == 0) {
            return 1;
        }
    }

    return 0;
}

int get_sunspots(FILE *f, const char *name, unsigned short *psunspots)
{
    record r;

    if (find_record(f, name, &r)) {
        *psunspots = r.sunspots;
        return 1;
    } else {
        return 0;
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Missing port number or pathname of customer file.\n");
        return 1;
    }
    int sfd;
    struct sockaddr_in a;
    FILE *f = fopen(argv[2], "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    // Bad clients can close before we write. Ignore SIGPIPE to keep running.
    ignore_sigpipe();

    sfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&a, 0, sizeof(struct sockaddr_in));
    a.sin_family = AF_INET;
    a.sin_port = htons(atoi(argv[1]));
    a.sin_addr.s_addr = htonl(INADDR_ANY);

    // Binds the socket to the specified address and port
    if (-1 == bind(sfd, (struct sockaddr *)&a, sizeof(struct sockaddr_in))) {
        if (errno == EADDRINUSE) {
            fprintf(stderr, "Error: Address already in use\n");
        } else {
            perror("bind");
        }
        exit(1);
    }

    // Prepares the socket to listen for incoming connections
    if (-1 == listen(sfd, 2)) { // may need to increase backlog..?
        perror("listen");
        return 1;
    }

    // Continually accept and handle new connections
    for (;;) {
        int cfd;
        struct sockaddr_in ca;
        socklen_t sinlen;

        sinlen = sizeof(struct sockaddr_in);
        cfd = accept(sfd, (struct sockaddr *)&ca, &sinlen);
        if (cfd == -1) {
            perror("accept");
            continue; // check for any new clients
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(cfd);
            continue;
        }

        if (pid == 0) { // child process
            close(sfd); // close listening socket..

            char buffer[30 + 1]; // +1 for null terminator
            size_t buffer_len = 0;
            ssize_t bytesRead;
            char *newline_pos;

            // Process client input until EOF or error
            for (;;) {
                bytesRead = read(cfd, buffer + buffer_len, sizeof(buffer) - buffer_len - 1);
                // buffer len reps current length of data alr in buffer
                // (need this, to deal with case of client skipper)
                if (bytesRead == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) { 
                        usleep(100000); // Sleep for 100ms before retrying
                        // means socket is non-blocking and there's no data available right now
                        continue;
                    } else {
                        perror("read");
                        close(cfd);
                        exit(1);
                    }
                } else if (bytesRead == 0) { // EOF - client closed the connection
                    fprintf(stderr, "Connection closed by client\n");
                    close(cfd);
                    exit(0); // Exit normally
                }

                buffer_len += bytesRead;
                buffer[buffer_len] = '\0'; // Null-terminate the buffer, being pre-cautious

                // Process each line
                while ((newline_pos = strchr(buffer, '\n')) != NULL) {
                    *newline_pos = '\0'; // Replace newline with null terminator!!

                    // Process the complete line
                    char name[30 + 1]; // includes new line and null terminator..
                    strncpy(name, buffer, sizeof(name));
                    name[sizeof(name) - 1] = '\0'; // replace new line with null terminator

                    unsigned short spots;
                    if (get_sunspots(f, name, &spots) == 1) {
                        spots = htons(spots); // Convert to network byte order

                        char response[11];
                        int len = snprintf(response, sizeof(response), "%u\n", ntohs(spots));
                        // we send this over to client

                        // Check for formatting errors
                        if (len < 0 || len >= sizeof(response)) {
                            perror("snprintf");
                            close(cfd);
                            exit(1);
                        }

                        // Send the response over the socket
                        if (write(cfd, response, len) == -1) {
                            perror("write");
                            close(cfd);
                            exit(1);
                        }
                    } else {
                        // Didn't find sunspots for given name, so we say "none"
                        if (write(cfd, "none\n", 5) == -1) {
                            perror("write");
                            close(cfd);
                            exit(1);
                        }
                    }

                    // Move remaining data to the start of the buffer!!!!
                    memmove(buffer, newline_pos + 1, buffer_len - (newline_pos - buffer) - 1);
                    buffer_len -= (newline_pos - buffer) + 1;
                    // for dealing with case client skipper
                }

                // Check if buffer is too full without a newline
                if (buffer_len >= sizeof(buffer) - 1) {
                    fprintf(stderr, "Client sent too much without newline\n");
                    close(cfd);
                    exit(1);
                }
            }
        } else { // parent process
            close(cfd); // Close client FD in child process
            while (waitpid(-1, NULL, WNOHANG) > 0) {
                // Dealing with zombies 
                // by checking for any child processes that have terminated
                usleep(100000); // Sleep for 100ms -> avoidomg busy polling
            }
        }
    }

    close(sfd);
    return 0;
}
