#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include "message.h"
#include "socket.h"

#define MAX_NAME_LEN 20

char *username;

void *send_message_func(void *port)
{
  while (true)
  {
    char message[MAX_MESSAGE_LENGTH];
    int c;
    int i = 0;
    while ((c = getchar()) != '\n') {
      message[i] = c;
      i++;
    }
    message[i] = '\0';

    int rc = send_message(*(int *)port, message);

    if (rc == -1)
    {
      perror("Failed to send message to server");
      exit(EXIT_FAILURE);
    }
    
  }
  return NULL;
}

void *accept_message(void *port)
{
  while (true) // continually loop until we receive a message successfully
  {
    char *message;

    // Read a message from the client
    message = receive_message(*(int *)port);
    if (message != NULL)
    {
      printf("%s", message);
      fflush(stdout);
    }
    free(message);
  }
  return NULL;
}

int main(int argc, char **argv)
{
  if (argc != 3)
  {
    fprintf(stderr, "Usage: %s<port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // Read command line arguments
  char *server_name = argv[1];
  unsigned short port = atoi(argv[2]);

  // Connect to the server
  int socket_fd = socket_connect(server_name, port);
  if (socket_fd == -1)
  {
    perror("Failed to connect");
    exit(EXIT_FAILURE);
  }
  pthread_t thread_accept_message, thread_send_message;
  pthread_create(&thread_accept_message, NULL, accept_message, &socket_fd);

  pthread_create(&thread_send_message, NULL, send_message_func, &socket_fd);
  while (true)
  {
  }
  return 0;
}