CC = gcc
CFLAGS = -pthread

OBJ = coins.o

all : coins_obj

coins_obj: $(OBJ)
	$(CC) $(CFLAGS) -o coins $(OBJ)

coins.o: coins.c

clean :
	rm -f coins *.o