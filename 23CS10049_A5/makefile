CC = gcc
CFLAGS = -Wall

all: manager timer launcher process

genburst: genbursts.c
	$(CC) $(CFLAGS) -o genbursts genbursts.c

manager: manager.c
	$(CC) $(CFLAGS) -o manager manager.c

timer: timer.c
	$(CC) $(CFLAGS) -o timer timer.c

launcher: launcher.c
	$(CC) $(CFLAGS) -o launcher launcher.c

process: process.c
	$(CC) $(CFLAGS) -o process process.c

run-burst: genbursts
	./genbursts

run: all
	./manager &
	sleep 0.5s
	./timer &
	sleep 0.5s
	./launcher 

clean:
	rm -f manager timer launcher process genburst 
