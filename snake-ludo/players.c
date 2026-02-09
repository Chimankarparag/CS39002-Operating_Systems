/******************************************************************************
 * players.c - Player Parent Process (PP) and Player Processes (A, B, C, ...)
 * * This file implements:
 * - PP: Player parent process that coordinates player turns
 * - Player processes: Each player that makes moves by rolling dice
 * * Flow:
 * - PP sends its PID to CP
 * - PP forks n player processes
 * - PP waits for SIGUSR1 from CP to initiate next move
 * - PP determines next player and sends SIGUSR1 to that player
 * - Player rolls dice, updates position, signals BP, and pauses
 * - PP terminates all players on SIGUSR2 from CP
 ******************************************************************************/
#define _POSIX_C_SOURCE 200112L 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define BOARD_SIZE 101      // Cells 0-100
#define MAX_PLAYERS 26      // Maximum players A-Z

// Global variables accessible by signal handlers and player function
int *MB;                    // Board shared memory
int *MP;                    // Positions shared memory
int n;                      // Number of players
int pipe_fd;                // Pipe write descriptor
pid_t bp_pid;               // Board process PID
pid_t *player_pids;         // Array of player process PIDs
int current_player = 0;     // Index of current player to move
volatile sig_atomic_t move_requested = 0;  // Flag for move request (PP)

// --- FIX: Moved 'my_turn' to global scope ---
volatile sig_atomic_t my_turn = 0;         // Flag for turn request (Player)

/******************************************************************************
 * Function: sigusr1_handler_pp
 * Description: Signal handler for PP - SIGUSR1 from CP to start next move
 ******************************************************************************/
void sigusr1_handler_pp(int sig) {
    move_requested = 1;
}

/******************************************************************************
 * Function: sigusr2_handler_pp
 * Description: Signal handler for PP - SIGUSR2 from CP to terminate
 ******************************************************************************/
void sigusr2_handler_pp(int sig) {
    // Terminate all player processes
    for (int i = 0; i < n; i++) {
        if (player_pids[i] > 0) {
            kill(player_pids[i], SIGUSR2);
        }
    }
    
    // Wait for all player processes with a delay for animation
    for (int i = 0; i < n; i++) {
        if (player_pids[i] > 0) {
            int status;
            waitpid(player_pids[i], &status, 0);
            printf("Player %c terminated.\n", 'A' + i);
            fflush(stdout);
            sleep(1);  // Slow animation as per spec
        }
    }
    
    // Detach from shared memory
    shmdt(MB);
    shmdt(MP);
    close(pipe_fd);
    
    free(player_pids);
    exit(0);
}

/******************************************************************************
 * Function: sigusr1_handler_player
 * Description: Signal handler for Player - SIGUSR1 from PP to take turn
 * --- FIX: Moved function to global scope ---
 ******************************************************************************/
void sigusr1_handler_player(int sig) {
    my_turn = 1;
}

/******************************************************************************
 * Function: roll_dice
 * Description: Simulates rolling a single die (1-6)
 * Returns: Random number between 1 and 6
 ******************************************************************************/
int roll_dice() {
    return (rand() % 6) + 1;
}

/******************************************************************************
 * Function: make_move
 * Description: Player makes a move by rolling dice and updating position
 * Parameters:
 * - player_idx: Index of the player (0 for A, 1 for B, etc.)
 ******************************************************************************/
void make_move(int player_idx) {
    char player_name = 'A' + player_idx;
    int pos = MP[player_idx];
    
    printf("\n--- Player %c's turn ---\n", player_name);
    printf("Current position: %d\n", pos);
    fflush(stdout);
    
    // If already at destination, do nothing
    if (pos == 100) {
        printf("Player %c is already at destination!\n", player_name);
        fflush(stdout);
        return;
    }
    
    // Roll dice with the three-6's cancellation rule
    int total = 0;
    int dice[3];
    int num_rolls = 0;
    
    while (1) {
        num_rolls = 0;
        total = 0;
        
        // First roll
        dice[num_rolls] = roll_dice();
        printf("Roll %d: %d", num_rolls + 1, dice[num_rolls]);
        total += dice[num_rolls];
        num_rolls++;
        
        if (dice[0] == 6) {
            // Second roll
            printf(" (got 6, rolling again)");
            fflush(stdout);
            dice[num_rolls] = roll_dice();
            printf(" -> Roll %d: %d", num_rolls + 1, dice[num_rolls]);
            total += dice[num_rolls];
            num_rolls++;
            
            if (dice[1] == 6) {
                // Third roll
                printf(" (got 6 again, rolling once more)");
                fflush(stdout);
                dice[num_rolls] = roll_dice();
                printf(" -> Roll %d: %d", num_rolls + 1, dice[num_rolls]);
                total += dice[num_rolls];
                num_rolls++;
                
                if (dice[2] == 6) {
                    // Three 6's - cancel and retry
                    printf(" X (Three 6's! Cancelled, rolling again)\n");
                    fflush(stdout);
                    continue;  // Retry the entire throw sequence
                }
            }
        }
        
        printf("\n");
        fflush(stdout);
        break;  // Valid throw sequence
    }
    
    printf("Total dice value: %d\n", total);
    fflush(stdout);
    
    // Calculate next position
    int nextpos = pos + total;
    
    // Check if move is valid
    if (nextpos > 100) {
        printf("Move exceeds 100 (would reach %d). Move not allowed.\n", nextpos);
        printf("Player %c stays at %d\n", player_name, pos);
        fflush(stdout);
        return;
    }
    
    // Check if destination cell is occupied by another player
    int occupied = 0;
    for (int p = 0; p < n; p++) {
        if (p != player_idx && MP[p] == nextpos && nextpos != 100) {
            occupied = 1;
            printf("Cell %d is occupied by Player %c. Move not allowed.\n", 
                   nextpos, 'A' + p);
            printf("Player %c stays at %d\n", player_name, pos);
            fflush(stdout);
            break;
        }
    }
    
    if (occupied) {
        return;
    }
    
    // Move is allowed, update position
    printf("Moving to cell %d", nextpos);
    fflush(stdout);
    
    // Follow ladders and snakes
    while (MB[nextpos] != 0) {
        if (MB[nextpos] > 0) {
            printf(" -> Ladder! Climbing to %d", nextpos + MB[nextpos]);
            nextpos += MB[nextpos];
        } else {
            printf(" -> Snake! Sliding to %d", nextpos + MB[nextpos]);
            nextpos += MB[nextpos];
        }
        fflush(stdout);
    }
    
    printf("\n");
    
    // Update position in shared memory
    MP[player_idx] = nextpos;
    printf("Player %c's new position: %d\n", player_name, nextpos);
    fflush(stdout);
    
    // Check if reached destination
    if (nextpos == 100) {
        printf("🎉 Player %c reached the destination! 🎉\n", player_name);
        fflush(stdout);
        
        // Decrement active player count
        MP[n]--;
        
        // Signal BP to print board
        kill(bp_pid, SIGUSR1);
        
        // Detach from shared memory before exiting
        shmdt(MB);
        shmdt(MP);
        close(pipe_fd);
        
        exit(0);  // Player exits after reaching destination
    }
}

/******************************************************************************
 * Function: player_main
 * Description: Main function for each player process (never returns)
 * Parameters:
 * - player_idx: Index of this player
 ******************************************************************************/
void player_main(int player_idx) {
    // Seed random number generator with PID and time
    srand(time(NULL) ^ getpid());
    
    // Set up signal handler for SIGUSR1 (move request from PP)
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler_player; // Uses the global function now
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    
    // Player loop: wait for turn signal, make move, signal BP
    while (1) {
        pause();  // Wait for SIGUSR1 from PP
        
        if (my_turn) {
            my_turn = 0;
            
            // Make the move
            make_move(player_idx);
            
            // Signal BP to print updated board (if not exited)
            kill(bp_pid, SIGUSR1);
        }
    }
    
    // Should never reach here
    exit(0);
}

/******************************************************************************
 * Function: main
 * Description: Main function for player parent process (PP)
 ******************************************************************************/
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_players> <pipe_fd> <bp_pid>\n", argv[0]);
        exit(1);
    }
    
    n = atoi(argv[1]);          // Number of players
    pipe_fd = atoi(argv[2]);    // Pipe write descriptor
    bp_pid = atoi(argv[3]);     // Board process PID
    
    // Send PP's PID to CP via pipe
    pid_t pp_pid = getpid();
    if (write(pipe_fd, &pp_pid, sizeof(pid_t)) != sizeof(pid_t)) {
        perror("PP: write PID to pipe");
        exit(1);
    }
    
    // Attach to shared memory segments
    key_t key_board = ftok(".", 'B');
    key_t key_pos = ftok(".", 'P');
    
    int shmid_board = shmget(key_board, BOARD_SIZE * sizeof(int), 0666);
    int shmid_pos = shmget(key_pos, (n + 1) * sizeof(int), 0666);
    
    if (shmid_board == -1 || shmid_pos == -1) {
        perror("PP: shmget");
        exit(1);
    }
    
    MB = (int *)shmat(shmid_board, NULL, 0);
    MP = (int *)shmat(shmid_pos, NULL, 0);
    
    if (MB == (int *)-1 || MP == (int *)-1) {
        perror("PP: shmat");
        exit(1);
    }
    
    // Allocate array for player PIDs
    player_pids = (pid_t *)malloc(n * sizeof(pid_t));
    
    // Set up signal handlers for PP
    struct sigaction sa1, sa2;
    
    sa1.sa_handler = sigusr1_handler_pp;
    sigemptyset(&sa1.sa_mask);
    sa1.sa_flags = 0;
    sigaction(SIGUSR1, &sa1, NULL);
    
    sa2.sa_handler = sigusr2_handler_pp;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = 0;
    sigaction(SIGUSR2, &sa2, NULL);
    
    // Fork player processes
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        
        if (pid == -1) {
            perror("fork player");
            exit(1);
        }
        
        if (pid == 0) {
            // Child process: player i
            player_main(i);  // Never returns
        }
        
        // Parent: store player PID
        player_pids[i] = pid;
    }
    
    printf("Player processes created:\n");
    for (int i = 0; i < n; i++) {
        printf("  Player %c: PID %d\n", 'A' + i, player_pids[i]);
    }
    fflush(stdout);
    
    // PP main loop: wait for move requests from CP
    while (1) {
        pause();  // Wait for SIGUSR1 from CP
        
        if (move_requested) {
            move_requested = 0;
            
            // Find next active player (one who hasn't reached destination)
            int attempts = 0;
            while (attempts < n) {
                if (MP[current_player] != 100) {
                    // This player is still in the game
                    break;
                }
                current_player = (current_player + 1) % n;
                attempts++;
            }
            
            // Check if any players are left
            if (MP[n] == 0) {
                // All players have finished
                continue;
            }
            
            // Send SIGUSR1 to the current player
            kill(player_pids[current_player], SIGUSR1);
            
            // Advance to next player for next round
            current_player = (current_player + 1) % n;
        }
    }
    
    // Should never reach here (terminated by SIGUSR2 handler)
    return 0;
}