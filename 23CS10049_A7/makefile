CC      = gcc
CFLAGS  = -Wall -Wextra -g

all: foobar

fooschedule.txt: run-genschedule
barschedule.txt: run-genschedule

genschedule: genschedule.c 
	$(CC) $(CFLAGS) -o genschedule genschedule.c

foobar: foobar.c manager.c work.c global.c 
	$(CC) $(CFLAGS) -o foobar genschedule.c

run-genschedule: genschedule
	./genschedule 16 20 3 5 fooschedule.txt barschedule.txt

rundemo: foobar fooschedule.txt barschedule.txt
	./foobar 2 1 fooschedule.txt barschedule.txt

clean:
	rm -f foobar work manager global

.PHONY: all demorun clean