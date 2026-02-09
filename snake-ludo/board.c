/******************************************************************************
 * board.c - Board Display Process (BP)
 * 
 * This process:
 * - Attaches to shared memory segments MB and MP
 * - Prints the initial board state
 * - Waits for SIGUSR1 signals from player processes
 * - Redraws the board after each move
 * - Sends acknowledgments to CP via pipe
 * - Terminates on SIGUSR2 from CP
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

#define BOARD_SIZE 101      // Cells 0-100
#define MAX_PLAYERS 26      // Maximum players A-Z

// Global variables for signal handler access
int *MB;                    // Board shared memory
int *MP;                    // Positions shared memory
int n;                      // Number of players
int pipe_fd;                // Pipe file descriptor for acknowledgments
volatile sig_atomic_t print_requested = 0;  // Flag for print request

/******************************************************************************
 * Function: sigusr1_handler
 * Description: Signal handler for SIGUSR1 - sets flag to print board
 ******************************************************************************/
void sigusr1_handler(int sig) {
    print_requested = 1;
}

/******************************************************************************
 * Function: print_cell
 * Description: Prints a single cell with appropriate formatting
 * Parameters:
 *   - cell: Cell number (1-100)
 *   - player_at_cell: Player character at this cell, or ' ' if empty
 ******************************************************************************/
void print_cell(int cell, char player_at_cell) {
    // Determine if cell has ladder or snake
    char symbol = ' ';
    if (MB[cell] > 0) {
        symbol = 'L';  // Ladder
    } else if (MB[cell] < 0) {
        symbol = 'S';  // Snake
    }
    
    if (player_at_cell != ' ') {
        // Player is at this cell
        printf(" [%c%c%3d] ", player_at_cell, symbol, cell);
    } else if (symbol != ' ') {
        // Ladder or snake at this cell
        printf(" [%c %3d] ", symbol, cell);
    } else {
        // Empty cell
        printf(" [  %3d] ", cell);
    }
}

/******************************************************************************
 * Function: print_board
 * Description: Prints the entire 10x10 board with current player positions
 ******************************************************************************/
void print_board() {
    // Clear screen for better visibility
    printf("\033[2J\033[H");  // ANSI escape codes: clear screen, move cursor to home
    
    printf("╔════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                          SNAKE LUDO GAME BOARD                             ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════╝\n\n");
    
    // Print players who have reached destination (cell 100)
    printf("Players at destination: ");
    int count_finished = 0;
    for (int p = 0; p < n; p++) {
        if (MP[p] == 100) {
            printf("%c ", 'A' + p);
            count_finished++;
        }
    }
    if (count_finished == 0) {
        printf("None");
    }
    printf("\n\n");
    
    // Print the 10x10 board (cells 1-100)
    // The board is printed in zigzag fashion: rows alternate left-to-right and right-to-left
    for (int row = 9; row >= 0; row--) {
        if (row % 2 == 1) {
            // Odd rows (from bottom): left to right (cells 91-100, 71-80, ...)
            for (int col = 0; col < 10; col++) {
                int cell = row * 10 + col + 1;
                
                // Find if any player is at this cell
                char player_char = ' ';
                for (int p = 0; p < n; p++) {
                    if (MP[p] == cell) {
                        player_char = 'A' + p;
                        break;
                    }
                }
                
                print_cell(cell, player_char);
            }
        } else {
            // Even rows: right to left (cells 81-90, 61-70, ...)
            for (int col = 9; col >= 0; col--) {
                int cell = row * 10 + col + 1;
                
                // Find if any player is at this cell
                char player_char = ' ';
                for (int p = 0; p < n; p++) {
                    if (MP[p] == cell) {
                        player_char = 'A' + p;
                        break;
                    }
                }
                
                print_cell(cell, player_char);
            }
        }
        printf("\n");
    }
    
    // Print legend
    printf("\n");
    printf("Legend: [L xxx] = Ladder  [S xxx] = Snake  [A xxx] = Player A at cell xxx\n");
    
    // Print players at home (cell 0)
    printf("\nPlayers at home: ");
    int count_home = 0;
    for (int p = 0; p < n; p++) {
        if (MP[p] == 0) {
            printf("%c ", 'A' + p);
            count_home++;
        }
    }
    if (count_home == 0) {
        printf("None");
    }
    printf("\n");
    
    // Print detailed ladder and snake information
    printf("\n");
    printf("Ladders and Snakes:\n");
    for (int cell = 1; cell < BOARD_SIZE; cell++) {
        if (MB[cell] > 0) {
            printf("  Ladder: %3d -> %3d\n", cell, cell + MB[cell]);
        } else if (MB[cell] < 0) {
            printf("  Snake:  %3d -> %3d\n", cell, cell + MB[cell]);
        }
    }
    
    printf("\n");
    printf("Active players: %d\n", MP[n]);
    printf("════════════════════════════════════════════════════════════════════════════\n");
    fflush(stdout);
}

/******************************************************************************
 * Function: main
 * Description: Main function for board process
 ******************************************************************************/
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_players> <pipe_fd>\n", argv[0]);
        exit(1);
    }
    
    n = atoi(argv[1]);          // Number of players
    pipe_fd = atoi(argv[2]);    // Pipe write descriptor
    
    // Get BP's own PID
    pid_t bp_pid = getpid();
    
    // Send BP's PID to CP via pipe
    if (write(pipe_fd, &bp_pid, sizeof(pid_t)) != sizeof(pid_t)) {
        perror("BP: write PID to pipe");
        exit(1);
    }
    
    // Sleep for 1 second to allow CP to read both PIDs
    sleep(1);
    
    // Attach to shared memory segments
    key_t key_board = ftok(".", 'B');
    key_t key_pos = ftok(".", 'P');
    
    int shmid_board = shmget(key_board, BOARD_SIZE * sizeof(int), 0666);
    int shmid_pos = shmget(key_pos, (n + 1) * sizeof(int), 0666);
    
    if (shmid_board == -1 || shmid_pos == -1) {
        perror("BP: shmget");
        exit(1);
    }
    
    MB = (int *)shmat(shmid_board, NULL, 0);
    MP = (int *)shmat(shmid_pos, NULL, 0);
    
    if (MB == (int *)-1 || MP == (int *)-1) {
        perror("BP: shmat");
        exit(1);
    }
    
    // Set up signal handler for SIGUSR1
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    
    // Print initial board
    print_board();
    
    // Send acknowledgment to CP
    char ack = 'B';
    if (write(pipe_fd, &ack, 1) != 1) {
        perror("BP: write initial ack");
        shmdt(MB);
        shmdt(MP);
        exit(1);
    }
    
    // Main loop: wait for signals
    while (1) {
        pause();  // Wait for signal
        
        if (print_requested) {
            print_requested = 0;
            
            // Print updated board
            print_board();
            
            // Send acknowledgment to CP
            if (write(pipe_fd, &ack, 1) != 1) {
                // CP may have closed pipe, time to exit
                break;
            }
        }
    }
    
    // Cleanup
    shmdt(MB);
    shmdt(MP);
    close(pipe_fd);
    
    return 0;
}
