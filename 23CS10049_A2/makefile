CC = gcc
CFLAGS = -Wall

all: agent customer

agent: agent.c
	$(CC) $(CFLAGS) -o agent agent.c

customer: customer.c
	$(CC) $(CFLAGS) -o customer customer.c

run: all
	./agent 15 5

clean:
	rm -f agent customer childpid.txt request.txt dummycpid.txt
