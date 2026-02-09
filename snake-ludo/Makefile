################################################################################
# Makefile for Snake Ludo Multi-Process Game
# 
# Targets:
#   all     - Compile all executables (default)
#   compile - Same as all
#   run     - Compile and run the game with 4 players
#   clean   - Remove executables and clean up IPC resources
################################################################################

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g

# Executables
TARGETS = ludo board players

# Default target
all: $(TARGETS)

# Compile target (same as all)
compile: $(TARGETS)

# Compile ludo (coordinator process)
ludo: ludo.c
	$(CC) $(CFLAGS) -o ludo ludo.c

# Compile board (board display process)
board: board.c
	$(CC) $(CFLAGS) -o board board.c

# Compile players (player parent and player processes)
players: players.c
	$(CC) $(CFLAGS) -o players players.c

# Run target - compile and execute with 4 players
run: $(TARGETS)
	./ludo 4

# Clean target - remove executables and clean up IPC resources
clean:
	rm -f $(TARGETS)
	@echo "Cleaning up shared memory segments..."
	@-ipcs -m | grep `whoami` | awk '{print $$2}' | xargs -I {} ipcrm -m {} 2>/dev/null || true
	@echo "Cleanup complete."

# Phony targets (not actual files)
.PHONY: all compile run clean
