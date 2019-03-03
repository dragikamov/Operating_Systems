CC = gcc
CFLAGS = -L. -levent -levent_core

OBJ = server.c tcp.c

all : server

server: $(OBJ)
	$(CC) -Wall -o server $(OBJ) $(CFLAGS)

clean :
	rm -f server *.o