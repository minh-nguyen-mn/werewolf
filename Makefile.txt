CC := clang
CFLAGS := -g  -Wall -Werror -Wno-unused-function -Wno-unused-variable  

all: server users

clean:
	rm -f server
	rm -f users

server: server.c socket.h message.h message.c util.h util.c
	$(CC) $(CFLAGS) -o  server server.c message.c util.c -fsanitize=address -lpthread

users: users.c message.h message.c
	$(CC) $(CFLAGS) -o  users users.c message.c -fsanitize=address -lpthread
