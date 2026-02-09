# Snake Ludo - Multi-Process Game Implementation

## Overview

This is a complete implementation of the Snake Ludo (Snakes and Ladders) game using multi-process programming with shared memory and signals for inter-process communication.

## Architecture

### Process Hierarchy
```
CP (Coordinator Process)
├── XBP (xterm for board) → BP (Board Process)
└── XPP (xterm for players) → PP (Player Parent)
                              ├── Player A
                              ├── Player B
                              ├── Player C
                              └── ...
```

### Processes

1. **CP (Coordinator Process)** - `ludo.c`
   - Creates and initializes shared memory segments
   - Handles user commands (next, autoplay, quit)
   - Coordinates game flow via signals
   - Manages cleanup and termination

2. **BP (Board Process)** - `board.c`
   - Displays the game board in a separate terminal
   - Updates board after each move
   - Sends acknowledgments to CP

3. **PP (Player Parent Process)** - `players.c`
   - Manages player turn sequence
   - Forks individual player processes
   - Routes move signals to active players

4. **Player Processes (A, B, C, ...)** - `players.c`
   - Each player rolls dice and makes moves
   - Updates position in shared memory
   - Signals BP for board update

## Shared Memory

### MB (Board Segment)
- 101 integers (cells 0-100)
- Cell 0: unused
- Cells 1-100: 
  - Positive value = ladder (difference from bottom to top)
  - Negative value = snake (difference from head to tail)
  - Zero = empty cell

### MP (Positions Segment)
- n+1 integers
- MP[0] to MP[n-1]: Player positions (0-100)
- MP[n]: Count of active players

## Communication

### Pipe (℘)
- BP → CP: PID and acknowledgments
- PP → CP: PID

### Signals
- **SIGUSR1**:
  - CP → PP: Initiate next move
  - PP → Player: Your turn to move
  - Player → BP: Print updated board
  
- **SIGUSR2**:
  - CP → PP: Terminate game
  - PP → Players: Terminate
  - CP → BP: Terminate

## Game Rules

### Board
- Standard 10×10 board (cells 1-100)
- Zigzag numbering pattern
- Cell 0 is "home" (invisible starting position)
- Cell 100 is the destination

### Dice Rolling
- Roll once: any face (1-6)
- If 6: roll again
- If second roll is 6: roll third time
- If third roll is also 6: **cancel all three rolls** and start over
- Move = sum of valid rolls (1, 2, or 3 dice)

### Move Validation
- Move blocked if:
  - Next position > 100
  - Destination cell occupied by another player
- After landing, automatically follow ladders/snakes until reaching empty cell

### Winning
- First player to reach cell 100 gets rank 1
- Game continues until all players reach destination
- Players exit upon reaching destination

## File Structure

```
.
├── ludo.c          # Coordinator process
├── board.c         # Board display process
├── players.c       # Player parent and player processes
├── ludo.txt        # Board configuration
├── Makefile        # Build system
└── README.md       # This file
```

## Building and Running

### Compilation
```bash
make compile
# or simply
make
```

This creates three executables:
- `ludo` - Coordinator
- `board` - Board display
- `players` - Player processes

### Running the Game
```bash
# Run with default 4 players
make run

# Or run manually with custom number of players (1-26)
./ludo 4
```

### Commands During Game

**Interactive Mode** (default):
- `next` - Execute next move
- `autoplay [delay_ms]` - Switch to autoplay mode with optional delay (default 1000ms)
- `quit` - End game early

**Autoplay Mode**:
- Game progresses automatically
- Delay between moves can be set (0 for fastest)
- Game ends when all players reach destination

### Cleanup
```bash
make clean
```
Removes executables and cleans up any orphaned shared memory segments.

## Implementation Details

### Synchronization
The game uses a carefully orchestrated sequence of blocking waits to ensure proper synchronization:

1. **Move Initiation**: CP → PP (signal)
2. **Player Selection**: PP → Player (signal)
3. **Move Execution**: Player updates position
4. **Board Update**: Player → BP (signal)
5. **Acknowledgment**: BP → CP (pipe)

Each step blocks until the next is ready, ensuring race-free operation.

### Signal Handlers

- **CP**: None (uses pipe for synchronization)
- **PP**: SIGUSR1 (next move), SIGUSR2 (terminate)
- **Players**: SIGUSR1 (make move)
- **BP**: SIGUSR1 (print board)

### Memory Management

All processes properly:
- Attach to shared memory when needed
- Detach before exiting
- CP removes segments only after all processes detach

### Error Handling

- Input validation for number of players
- File I/O error checking
- IPC creation and operation error handling
- Graceful cleanup on errors

## Board Configuration (ludo.txt)

Format:
```
L <from> <to>    # Ladder from cell <from> to cell <to>
S <from> <to>    # Snake from cell <from> to cell <to>
E                # End of board definition
```

Example:
```
L 3 21           # Ladder: 3 → 21
S 29 7           # Snake: 29 → 7
E
```

## Display Features

### Board Window (Green Background)
- 10×10 grid showing all cells
- Ladders marked with 'L'
- Snakes marked with 'S'
- Players shown at their positions
- Legend and statistics

### Players Window (Blue Background)
- Move-by-move transcript
- Dice roll results
- Position updates
- Ladder/snake encounters
- Destination arrivals

## Technical Notes

### xterm Configuration
- **Board**: 150 columns × 24 rows, position (50, 100)
- **Players**: 100 columns × 24 rows, position (1000, 100)
- Custom background colors for visual distinction
- Font size 15 for readability

### Process Termination Sequence
1. User quits or all players finish
2. CP signals PP to terminate
3. PP signals all active players
4. PP waits for player zombies (with 1s delays)
5. PP exits → XPP terminates
6. CP signals BP to terminate
7. BP exits → XBP terminates
8. CP cleans up shared memory and exits

### Race Condition Prevention
- Sequential player moves (no concurrent dice rolling)
- Pipe-based acknowledgments ensure synchronization
- Atomic updates to shared memory at non-overlapping locations
- MP[n] is the only cell with potential concurrent writes (decremented by exiting players)

## Testing Recommendations

1. **Basic Flow**: Run with 2-4 players, use `next` command
2. **Autoplay**: Test with different delays (0, 100, 1000ms)
3. **Edge Cases**:
   - Single player game
   - Maximum players (26)
   - Early quit
   - Three consecutive 6's
   - Blocked moves (cell occupied, exceed 100)
   - Multiple ladder/snake encounters in one move

## Known Limitations

- Requires X11 for xterm display
- Maximum 26 players (A-Z)
- No persistence (game state lost on exit)
- Terminal size must accommodate display

## Author

This implementation follows the CS39002 Operating Systems Laboratory Assignment 4 specification for Spring 2026.

## License

Educational use only - part of academic coursework.
