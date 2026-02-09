/******************************************************************************
 * ludo.c - Coordinator Process (CP)
 * 
 * This is the main coordinator process that:
 * - Creates shared memory segments for board (MB) and player positions (MP)
 * - Initializes the board from ludo.txt
 * - Forks xterm processes for board display (XBP) and players (XPP)
 * - Handles user commands (next, autoplay, quit)
 * - Coordinates the game flow via signals and pipes
 * - Cleans up resources at the end
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>

#define BOARD_SIZE 101      // Cells 0-100, where 0 is home
#define MAX_PLAYERS 26      // Maximum players A-Z
#define PIPE_READ 0         // Pipe read end index
#define PIPE_WRITE 1        // Pipe write end index

/******************************************************************************
 * Function: read_board
 * Description: Reads the ludo board from ludo.txt and populates MB
 * Parameters:
 *   - board: Pointer to shared memory segment MB
 * Returns: 0 on success, -1 on error
 ******************************************************************************/
int read_board(int *board) {
    FILE *fp = fopen("ludo.txt", "r");
    if (!fp) {
        perror("Error opening ludo.txt");
        return -1;
    }
    
    // Initialize all cells to 0 (no ladder/snake)
    for (int i = 0; i < BOARD_SIZE; i++) {
        board[i] = 0;
    }
    
    char type;
    int from, to;
    
    // Read each line: L/S from to or E for end
    while (fscanf(fp, " %c", &type) == 1) {
        if (type == 'E') {
            break;  // End of board definition
        }
        
        if (type == 'L' || type == 'S') {
            if (fscanf(fp, "%d %d", &from, &to) == 2) {
                // Store difference: positive for ladder, negative for snake
                board[from] = to - from;
            }
        }
    }
    
    fclose(fp);
    return 0;
}

/******************************************************************************
 * Function: main
 * Description: Main coordinator process
 ******************************************************************************/
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_players>\n", argv[0]);
        exit(1);
    }
    
    int n = atoi(argv[1]);  // Number of players
    if (n < 1 || n > MAX_PLAYERS) {
        fprintf(stderr, "Number of players must be between 1 and %d\n", MAX_PLAYERS);
        exit(1);
    }
    
    printf("Snake Ludo Game\n");
    printf("===============\n");
    printf("Number of players: %d\n\n", n);
    
    // Create unique keys for shared memory segments
    key_t key_board = ftok(".", 'B');  // Key for board segment
    key_t key_pos = ftok(".", 'P');    // Key for positions segment
    
    // Create shared memory segment for board (101 integers)
    int shmid_board = shmget(key_board, BOARD_SIZE * sizeof(int), 
                              IPC_CREAT | IPC_EXCL | 0666);
    if (shmid_board == -1) {
        perror("shmget for board");
        exit(1);
    }
    
    // Create shared memory segment for player positions (n+1 integers)
    int shmid_pos = shmget(key_pos, (n + 1) * sizeof(int), 
                            IPC_CREAT | IPC_EXCL | 0666);
    if (shmid_pos == -1) {
        perror("shmget for positions");
        shmctl(shmid_board, IPC_RMID, NULL);  // Clean up board segment
        exit(1);
    }
    
    // Attach to shared memory segments
    int *MB = (int *)shmat(shmid_board, NULL, 0);
    int *MP = (int *)shmat(shmid_pos, NULL, 0);
    
    if (MB == (int *)-1 || MP == (int *)-1) {
        perror("shmat");
        shmctl(shmid_board, IPC_RMID, NULL);
        shmctl(shmid_pos, IPC_RMID, NULL);
        exit(1);
    }
    
    // Initialize the board from ludo.txt
    if (read_board(MB) == -1) {
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_board, IPC_RMID, NULL);
        shmctl(shmid_pos, IPC_RMID, NULL);
        exit(1);
    }
    
    // Initialize player positions: all at home (0), and n players active
    for (int i = 0; i < n; i++) {
        MP[i] = 0;  // All players start at home
    }
    MP[n] = n;  // Number of active players
    
    // Create pipe for communication from BP and PP to CP
    int pfd[2];
    if (pipe(pfd) == -1) {
        perror("pipe");
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_board, IPC_RMID, NULL);
        shmctl(shmid_pos, IPC_RMID, NULL);
        exit(1);
    }
    
    // Convert pipe write descriptor and n to strings for command-line args
    char pfd_write_str[20], n_str[20];
    sprintf(pfd_write_str, "%d", pfd[PIPE_WRITE]);
    sprintf(n_str, "%d", n);
    
    // Fork XBP (xterm for board display)
    pid_t xbp_pid = fork();
    if (xbp_pid == -1) {
        perror("fork XBP");
        close(pfd[PIPE_READ]);
        close(pfd[PIPE_WRITE]);
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_board, IPC_RMID, NULL);
        shmctl(shmid_pos, IPC_RMID, NULL);
        exit(1);
    }
    
    if (xbp_pid == 0) {
        // XBP process: execute xterm for board
        close(pfd[PIPE_READ]);  // XBP doesn't need read end
        
        execlp("xterm", "xterm", 
               "-T", "Board",           // Title
               "-fs", "15",             // Font size
               "-geometry", "150x24+50+100",  // Columns x Rows + X + Y
               "-bg", "#003300",        // Dark green background
               "-e", "./board", n_str, pfd_write_str,
               NULL);
        
        // If execlp fails
        perror("execlp xterm for board");
        exit(1);
    }
    
    // CP continues: Read BP's PID from pipe
    pid_t bp_pid;
    if (read(pfd[PIPE_READ], &bp_pid, sizeof(pid_t)) != sizeof(pid_t)) {
        perror("read BP PID");
        kill(xbp_pid, SIGKILL);
        waitpid(xbp_pid, NULL, 0);
        close(pfd[PIPE_READ]);
        close(pfd[PIPE_WRITE]);
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_board, IPC_RMID, NULL);
        shmctl(shmid_pos, IPC_RMID, NULL);
        exit(1);
    }
    
    // Convert BP PID to string for XPP's arguments
    char bp_pid_str[20];
    sprintf(bp_pid_str, "%d", bp_pid);
    
    // Fork XPP (xterm for players)
    pid_t xpp_pid = fork();
    if (xpp_pid == -1) {
        perror("fork XPP");
        kill(xbp_pid, SIGKILL);
        waitpid(xbp_pid, NULL, 0);
        close(pfd[PIPE_READ]);
        close(pfd[PIPE_WRITE]);
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_board, IPC_RMID, NULL);
        shmctl(shmid_pos, IPC_RMID, NULL);
        exit(1);
    }
    
    if (xpp_pid == 0) {
        // XPP process: execute xterm for players
        close(pfd[PIPE_READ]);  // XPP doesn't need read end
        
        execlp("xterm", "xterm",
               "-T", "Players",         // Title
               "-fs", "15",             // Font size
               "-geometry", "100x24+1000+100",  // Columns x Rows + X + Y
               "-bg", "#000033",        // Dark blue background
               "-e", "./players", n_str, pfd_write_str, bp_pid_str,
               NULL);
        
        // If execlp fails
        perror("execlp xterm for players");
        exit(1);
    }
    
    // CP continues: Read PP's PID from pipe
    pid_t pp_pid;
    if (read(pfd[PIPE_READ], &pp_pid, sizeof(pid_t)) != sizeof(pid_t)) {
        perror("read PP PID");
        kill(xpp_pid, SIGKILL);
        kill(xbp_pid, SIGKILL);
        waitpid(xpp_pid, NULL, 0);
        waitpid(xbp_pid, NULL, 0);
        close(pfd[PIPE_READ]);
        close(pfd[PIPE_WRITE]);
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_board, IPC_RMID, NULL);
        shmctl(shmid_pos, IPC_RMID, NULL);
        exit(1);
    }
    
    // Wait for initial board print acknowledgment from BP
    char ack;
    if (read(pfd[PIPE_READ], &ack, 1) != 1) {
        perror("read initial ack from BP");
        kill(xpp_pid, SIGKILL);
        kill(xbp_pid, SIGKILL);
        waitpid(xpp_pid, NULL, 0);
        waitpid(xbp_pid, NULL, 0);
        close(pfd[PIPE_READ]);
        close(pfd[PIPE_WRITE]);
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_board, IPC_RMID, NULL);
        shmctl(shmid_pos, IPC_RMID, NULL);
        exit(1);
    }
    
    // Close write end of pipe (only children write to it)
    close(pfd[PIPE_WRITE]);
    
    printf("Game initialized successfully!\n");
    printf("Commands: next, autoplay, quit\n\n");
    
    // Game loop
    int autoplay = 0;       // Autoplay mode flag
    int delay_ms = 1000;    // Default delay in milliseconds
    char command[100];
    
    while (1) {
        if (autoplay) {
            // In autoplay mode, sleep for delay and auto-trigger next move
            usleep(delay_ms * 1000);  // Convert ms to microseconds
            
            // Check if game is over
            if (MP[n] == 0) {
                printf("\nAll players have reached the destination!\n");
                printf("Press return to exit: ");
                fflush(stdout);
                getchar();
                break;
            }
            
            // Trigger next move
            kill(pp_pid, SIGUSR1);
            
            // Wait for acknowledgment from BP
            if (read(pfd[PIPE_READ], &ack, 1) != 1) {
                break;  // BP may have terminated
            }
        } else {
            // Interactive mode: wait for user command
            printf("Enter command: ");
            fflush(stdout);
            
            if (fgets(command, sizeof(command), stdin) == NULL) {
                break;
            }
            
            // Remove newline
            command[strcspn(command, "\n")] = 0;
            
            if (strcmp(command, "next") == 0) {
                // Check if game is over
                if (MP[n] == 0) {
                    printf("Game is already over!\n");
                    printf("Press return to exit: ");
                    fflush(stdout);
                    getchar();
                    break;
                }
                
                // Send signal to PP to initiate next move
                kill(pp_pid, SIGUSR1);
                
                // Wait for acknowledgment from BP
                if (read(pfd[PIPE_READ], &ack, 1) != 1) {
                    break;  // BP may have terminated
                }
            } else if (strncmp(command, "autoplay", 8) == 0) {
                // Parse optional delay
                int custom_delay;
                if (sscanf(command, "autoplay %d", &custom_delay) == 1) {
                    delay_ms = custom_delay;
                }
                printf("Entering autoplay mode with delay %d ms\n", delay_ms);
                autoplay = 1;
            } else if (strcmp(command, "quit") == 0) {
                printf("Quitting game...\n");
                printf("Press return to exit: ");
                fflush(stdout);
                getchar();
                break;
            } else {
                printf("Unknown command: %s\n", command);
                printf("Valid commands: next, autoplay [delay_ms], quit\n");
            }
        }
    }
    
    // Cleanup phase
    printf("\nCleaning up...\n");
    
    // Send termination signal to PP
    kill(pp_pid, SIGUSR2);
    
    // Wait for XPP to terminate (which happens after PP terminates)
    waitpid(xpp_pid, NULL, 0);
    printf("Player processes terminated.\n");
    
    // Send termination signal to BP
    kill(bp_pid, SIGUSR2);
    
    // Wait for XBP to terminate (which happens after BP terminates)
    waitpid(xbp_pid, NULL, 0);
    printf("Board process terminated.\n");
    
    // Close pipe
    close(pfd[PIPE_READ]);
    
    // Detach from shared memory
    shmdt(MB);
    shmdt(MP);
    
    // Remove shared memory segments
    shmctl(shmid_board, IPC_RMID, NULL);
    shmctl(shmid_pos, IPC_RMID, NULL);
    
    printf("Game ended successfully.\n");
    
    return 0;
}
