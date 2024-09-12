#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>

#include "record.h"

#define MAX_NAME_LEN 29
#define MAX_SUNSPOT_LEN 10

void sigchld_handler(int signo) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void setup_signal_handlers() {
    struct sigaction sa;

    // Ignore SIGPIPE
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, NULL);

    // SIGCHLD
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

void print_client_address(const char *prefix, const struct sockaddr_in *ptr)
{
  char dot_notation[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &ptr->sin_addr, dot_notation, INET_ADDRSTRLEN);
  fprintf(stderr, "%s: %s port %d\n", prefix, dot_notation, ntohs(ptr->sin_port));
}

int get_sunspots(FILE *f, const char *name, unsigned short *psunspots)
{
    if (f == NULL || name == NULL) {
        return 0;
    }

    fseek(f, 0, SEEK_SET);

    record rec;
    while (fread(&rec, sizeof(record), 1, f) == 1) {
        if ((rec.name_len == strlen(name) || rec.name_len == NAME_LEN_MAX) && strncmp(rec.name, name, rec.name_len) == 0) {
            *psunspots = rec.sunspots;
            return 1; //return after first match
        }
    }

    return 0;
}

void handle_client(int cfd, const char *customer_file_path) {
    char name[MAX_NAME_LEN + 2]; // +2 for newline and null terminator
    char response[MAX_SUNSPOT_LEN + 2]; // +2 for newline and null terminator

    FILE *client_stream = fdopen(cfd, "r");
    if (client_stream == NULL) {
        perror("fdopen");
        close(cfd);
        return;
    }

    FILE *customer_file = fopen(customer_file_path, "r");
    if (customer_file == NULL) {
        perror("fopen");
        close(cfd);
        fclose(client_stream);
        return;
    }

    while (fgets(name, sizeof(name), client_stream) != NULL) {
        // check newline termination
        size_t len = strlen(name);
        if (len == 0 || len > MAX_NAME_LEN + 1 || name[len - 1] != '\n' || name[0] == '\n') {
            fprintf(stderr, "Received message too long\n");
            break;
        }

        // Remove newline character
        name[len - 1] = '\0';

        // Look up the customer file -> copy
        // sleep(2); 

        unsigned short sunspots; 
        if (get_sunspots(customer_file, name, &sunspots)) {
            snprintf(response, sizeof(response), "%d\n", sunspots);
        } else {
            snprintf(response, sizeof(response), "none\n");
        }

        if (write(cfd, response, strlen(response)) == -1) {
            if (errno == EPIPE) {
                fprintf(stderr, "Connection closed by server.\n");
            }
            perror("write");
            break;
        }
    }
    fclose(customer_file);
    fclose(client_stream);
    close(cfd);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Need 2 arguments: port customer-file\n");
        return 1;
    }

    int port = atoi(argv[1]);
    const char *customer_file_path = argv[2];

    struct sockaddr_in a;
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        perror("socket");
        return 1;
    }

    memset(&a, 0, sizeof(struct sockaddr_in));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sfd, (struct sockaddr *)&a, sizeof(struct sockaddr_in)) == -1) {
        perror("bind");
        close(sfd);
        return 1;
    }

    if (-1 == listen(sfd, 2)) {
        perror("listen");
        close(sfd);
        return 1;
    }

    setup_signal_handlers();

    for (;;) {
        struct sockaddr_in ca; /// do i need
        socklen_t sinlen=sinlen = sizeof(struct sockaddr_in);

        int cfd = accept(sfd, (struct sockaddr *)&ca, &sinlen);
        if (cfd == -1) {
            perror("accept");
            continue;
        }
        print_client_address("Got client", &ca);
        
        //Do stuff
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(cfd);
            continue;
        } else if(pid == 0) { //Child
            close(sfd);
            handle_client(cfd, customer_file_path);
            exit(0);
        } else {
            close(cfd);
        }
    }

    close(sfd);
    return 0;
}