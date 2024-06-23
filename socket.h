#if !defined(SOCKET_H)
#define SOCKET_H

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

/**
 * Create a new socket and connect to a server.
 *
 * \param server_name   A null-terminated string that specifies either the IP
 *                      address or host name of the server to connect to.
 * \param port          The port number the server should be listening on.
 *
 * \returns   A file descriptor for the connected socket, or -1 if there is an
 *            error. The errno value will be set by the failed POSIX call.
 */
static int socket_connect(char* server_name, unsigned short port) {
  // Look up the server by name
  struct hostent* server = gethostbyname(server_name);
  /*if (server == NULL) {
    // Set errno, since gethostbyname does not
    errno = EHOSTDOWN;
    return -1;
  }*/

  // Open a socket
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    return -1;
  }

  // Set up an address
  struct sockaddr_in addr = {
      .sin_family = AF_INET,   // This is an internet socket
      .sin_port = htons(port)  // Connect to the appropriate port number
  };

  // Copy the server address info returned by gethostbyname into the address
  memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);

  // Connect to the server
  if (connect(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {
    close(fd);
    return -1;
  }

  return fd;
}

/**
 * Open a server socket that will accept TCP connections from any other machine.
 *
 * \param port    A pointer to a port value. If *port is greater than zero, this
 *                function will attempt to open a server socket using that port.
 *                If *port is zero, the OS will choose. Regardless of the method
 *                used, this function writes the socket's port number to *port.
 *
 * \returns       A file descriptor for the server socket. The socket has been
 *                bound to a particular port and address, but is not listening.
 *                In case of failure, this function returns -1. The value of
 *                errno will be set by the POSIX socket function that failed.
 */
static int server_socket_open(unsigned short* port) {
  // Create a server socket. Return if there is an error.
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    return -1;
  }

  // Set up the server socket to listen
  struct sockaddr_in addr = {
      .sin_family = AF_INET,          // This is an internet socket
      .sin_addr.s_addr = INADDR_ANY,  // Listen for connections from any client
      .sin_port = htons(*port)        // Use the specified port (may be zero)
  };

  // Bind the server socket to the address. Return if there is an error.
  if (bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {
    close(fd);
    return -1;
  }

  // Get information about the new socket
  socklen_t addrlen = sizeof(struct sockaddr_in);
  if (getsockname(fd, (struct sockaddr*)&addr, &addrlen)) {
    close(fd);
    return -1;
  }

  // Read out the port information for the socket. If *port was zero, the OS
  // will select a port for us. This tells the caller which port was chosen.
  *port = ntohs(addr.sin_port);

  // Return the server socket file descriptor
  return fd;
}

/**
 * Accept an incoming connection on a server socket.
 *
 * \param server_socket_fd  The server socket that should accept the connection.
 *
 * \returns   The file descriptor for the newly-connected client socket. In case
 *            of failure, returns -1 with errno set by the failed accept call.
 */
static int server_socket_accept(int server_socket_fd) {
  // Create a struct to record the connected client's address
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(struct sockaddr_in);

  // Block until we receive a connection or failure
  int client_socket_fd = accept(server_socket_fd, (struct sockaddr*)&client_addr, &client_addr_len);

  // Did something go wrong?
  if (client_socket_fd == -1) {
    return -1;
  }

  return client_socket_fd;
}

#endif
