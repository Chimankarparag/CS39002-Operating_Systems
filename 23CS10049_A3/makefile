CC = gcc
CFLAGS = -Wall

all: rundfa gendfa

rundfa: rundfa.c
	$(CC) $(CFLAGS) -o rundfa rundfa.c

gendfa: gendfa.c
	$(CC) $(CFLAGS) -o gendfa gendfa.c

verbose: rundfa.c
	$(CC) $(CFLAGS) -D_VERBOSE -o rundfa rundfa.c

run-verbose: verbose
	./rundfa demodfa.txt

run-gendfa: gendfa
	./gendfa

clean:
	rm -f rundfa gendfa