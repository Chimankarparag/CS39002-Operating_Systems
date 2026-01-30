CC = gcc
CFLAGS = -Wall

all: rundfa gendfa

rundfa: rundfa.c
	$(CC) $(CFLAGS) -o rundfa rundfa.c

gendfa: gendfa.c
	$(CC) $(CFLAGS) -o gendfa gendfa.c

verbose: rundfa.c
	$(CC) $(CFLAGS) -D_VERBOSE -o rundfa rundfa.c

verbose-chat: chat2.c
	$(CC) $(CFLAGS) -D_VERBOSE -o rundfa chat2.c

run-verbose: verbose-chat
	./rundfa thirdlast.txt

run-gendfa: gendfa
	./gendfa

clean:
	rm -f rundfa gendfa