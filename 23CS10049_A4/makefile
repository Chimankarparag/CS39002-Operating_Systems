CC = gcc
CFLAGS = -Wall -Wextra -g

TARGETS = ludo board players

all: $(TARGETS)

compile: $(TARGETS)

ludo: ludo.c
	$(CC) $(CFLAGS) -o ludo ludo.c

board: board.c
	$(CC) $(CFLAGS) -o board board.c

players: players.c
	$(CC) $(CFLAGS) -o players players.c

run: $(TARGETS)
	./ludo 4

clean:
	rm -f $(TARGETS)
	@echo "Cleaning up shared memory segments..."
	@-ipcs -m | grep `whoami` | awk '{print $$2}' | xargs -I {} ipcrm -m {} 2>/dev/null || true
	@echo "Cleanup complete."
	
.PHONY: all compile run clean
