CC      = gcc
CFLAGS  = -Wall -Wextra -g

all: manager demon nomad

manager: manager.c cond.c house.h
	$(CC) $(CFLAGS) -o manager manager.c

demon: demon.c cond.c house.h
	$(CC) $(CFLAGS) -o demon demon.c

nomad: nomad.c cond.c house.h
	$(CC) $(CFLAGS) -o nomad nomad.c

demorun: all
	./demon 4 &
	./nomad 5

clean:
	rm -f manager demon nomad

.PHONY: all demorun clean