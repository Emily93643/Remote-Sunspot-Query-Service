#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#define BUFFER_SIZE 30

void parse_inaddr(struct addrinfo *ai, const char *hostname, const char *port) {
  struct addrinfo hint;
  struct addrinfo *head;

  memset(&hint, 0, sizeof(struct addrinfo));
  hint.ai_family = AF_INET;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_protocol = 0;
  hint.ai_flags = AI_NUMERICSERV;

  int r = getaddrinfo(hostname, port, &hint, &head);
  if (r != 0) {
    if (r == EAI_SYSTEM) {
      perror("getaddrinfo");
    } else {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
    }
    exit(1);
  } else {
    *ai = *head;
    ai->ai_next = NULL;
    freeaddrinfo(head);
  }
}

void handle_sigpipe(int sig) {
    fprintf(stderr, "SIGPIPE received: Connection closed by the server\n");
    exit(1);
}

int main(int argc, char **argv)
{
  signal(SIGPIPE, handle_sigpipe);
  if (argc < 3) {
    fprintf(stderr, "Need IPv4 address and port.\n");
    return 1;
  }

  struct addrinfo ai;
  parse_inaddr(&ai, argv[1], argv[2]);

  int s = socket(ai.ai_family, ai.ai_socktype, ai.ai_protocol);
  if (s == -1) {
    perror("socket");
    return 1;
  }

  if (-1 == connect(s, ai.ai_addr, ai.ai_addrlen)) {
    perror("connect");
    close(s);
    return 1;
  }

  printf("Ready\n");
  char line[BUFFER_SIZE + 1];

  while(fgets(line, sizeof(line), stdin) != NULL){
    //You may assume that it is at most 30 bytes including newline
    // if (strlen(line) > 30) {
    //   fprintf(stderr, "Input too long\n");
    //   close(s);
    //   return 1;
    // }
    // //If EOF or a blank line (only newline), close the connection and exit code 0.
    if(strlen(line) == 1 && line[0] == '\n'){
      close(s);
      return 0;
    }

    //Send the name to the server. 
    if (write(s, line, strlen(line)) == -1) {
      perror("write");
      close(s);
      return 1;
    }
    
    //Print the server’s reply to stdout. 
    //Don’t print more than one newline.
    // char response[1024];
    // ssize_t n = read(s, response, sizeof(response) - 1);
    // if (n == -1) {
    //   perror("read");
    //   close(s);
    //   return 1;
    // }

    char response[1024];
    ssize_t n;
    int total_read = 0;
    while ((n = read(s, response + total_read, sizeof(response) - 1 - total_read)) > 0) {
      total_read += n;
      response[total_read] = '\0';
      if (strchr(response, '\n') != NULL) {
        break;
      }
    }

    // response[n] = '\0';
    // if (n == 0 || (n > 11 && response[n-1] != '\n')) {
    //   fprintf(stderr, "Not valid reply! Server response too long\n");
    //   close(s);
    //   return 1;
    // }

    if (n == -1) {
      perror("read");
      close(s);
      return 1;
    }

    if (total_read == 0 || total_read > 11) {
      fprintf(stderr, "Invalid response from server.\n");
      close(s);
      return 1;
    }

    printf("%s", response);
  }
  if (ferror(stdin)) {
    perror("fgets");
  }
  close(s);
  return 0;
}