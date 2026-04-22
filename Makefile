CC = gcc
CFLAGS = -Wall -Wextra -g -O2
CFLAGS_PIC = $(CFLAGS) -fPIC
LDFLAGS_MAIN = -pthread -lrt -lm
LDFLAGS_CHILD = -lncurses -pthread -lrt -lm

all: static shared
static: main_static child_static
shared: main_shared child_shared

main_static: main.o libparent.a
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_MAIN)

child_static: child.o librender.a
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_CHILD)

main_shared: main.o libparent.so
	$(CC) $(CFLAGS) -o $@ $< -L. -lparent $(LDFLAGS_MAIN) -Wl,-rpath=.

child_shared: child.o librender.so
	$(CC) $(CFLAGS) -o $@ $< -L. -lrender $(LDFLAGS_CHILD) -Wl,-rpath=.

lib%.a: %.o
	ar rcs $@ $^

libparent.so: parent_pic.o
	$(CC) -shared -o $@ $^ $(LDFLAGS_MAIN)

librender.so: render_pic.o
	$(CC) -shared -o $@ $^ $(LDFLAGS_CHILD)

main.o: main.c parent.h common.h
child.o: child.c render.h common.h
parent.o parent_pic.o: parent.c parent.h common.h
render.o render_pic.o: render.c render.h common.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%_pic.o: %.c
	$(CC) $(CFLAGS_PIC) -c $< -o $@

clean:
	rm -f main_static main_shared child_static child_shared *.o *.a *.so
