CC = gcc
CFLAGS = -Wall -Wextra -g -O2

LDFLAGS_MAIN = -pthread -lrt -lm

LDFLAGS_CHILD = -lncurses -pthread -lrt -lm

all: main child

main: main.c parent.c parent.h common.h
	$(CC) $(CFLAGS) -o main main.c parent.c $(LDFLAGS_MAIN)

child: child.c render.c render.h common.h
	$(CC) $(CFLAGS) -o child child.c render.c $(LDFLAGS_CHILD)

clean:
	rm -f main child
