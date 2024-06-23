#pragma once

#define MAX_MESSAGE_LENGTH 2048

// Send a across a socket with a header that includes the message length. Returns non-zero value if
// an error occurs.
int send_message(int fd, char* message);

// Receive a message from a socket and return the message string (which must be freed later).
// Returns NULL when an error occurs.
char* receive_message(int fd);
