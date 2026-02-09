#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

#define BOARD_SIZE 101
#define MAX_PLAYERS 26  

int *MB;
int *MP;
int num_players;
int CP_pfd_write;

volatile sig_atomic_t print_requested = 0;

void sigusr1_handler(int sig) {
    print_requested = 1;
}
void print_cell_content(int cell) {
    int occupied = -1;
    for (int i = 0; i < num_players; i++) {
        if (MP[i] == cell && cell != 100) {
            occupied = i;
            break;
        }
    }

    if (occupied != -1) {
        printf(" %c |", 'A' + occupied);
    } else {
        printf("%3d|", cell);
    }
}

void print_board() {
    printf("\033[H"); 

    for (int i = 0; i < num_players; i++) {
        if (MP[i] == 100) {
            printf("%c ", 'A' + i);
        }
    }
    printf("                                \n"); 

    for (int row = 9; row >= 0; row--) {
        printf("+---+---+---+---+---+---+---+---+---+---+\n");

        printf("|");
        if (row % 2 == 0) { 
            for (int col = 0; col < 10; col++) {
                int cell = row * 10 + col + 1;
                print_cell_content(cell);
            }
        } else { 
            for (int col = 9; col >= 0; col--) {
                int cell = row * 10 + col + 1;
                print_cell_content(cell);
            }
        }
        printf("\t"); 
        for (int col = 0; col < 10; col++) {
            int cell = row * 10 + col + 1;
            if (MB[cell] > 0) {
                printf("L(%2d -> %2d) ", cell, cell + MB[cell]);
            } else if (MB[cell] < 0) {
                printf("S(%2d -> %2d) ", cell, cell + MB[cell]);
            }
        }
        printf("\n");
    }
    printf("+---+---+---+---+---+---+---+---+---+---+\n");
    for (int i = 0; i < num_players; i++) {
        if (MP[i] == 0) {
            printf("%c ", 'A' + i);
        }
    }
    printf("\033[J"); 
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_players> <pipe_fd>\n", argv[0]);
        exit(1);
    }
    
    num_players = atoi(argv[1]);
    CP_pfd_write = atoi(argv[2]);
    
    pid_t bp_pid = getpid();
    if (write(CP_pfd_write, &bp_pid, sizeof(pid_t)) != sizeof(pid_t)) {
        perror("BP: write PID");
        exit(1);
    }
    
    sleep(1);
    
    key_t key_MB = ftok("ludo.txt", 'B');
    key_t key_MP = ftok("ludo.txt", 'P');

    
    int shmid_MB = shmget(key_MB, BOARD_SIZE * sizeof(int), 0666);
    int shmid_MP = shmget(key_MP, (num_players+ 1) * sizeof(int), 0666);
    
    if (shmid_MB == -1 || shmid_MP == -1) {
        perror("BP: shmget");
        exit(1);
    }
    
    MB = (int *)shmat(shmid_MB, NULL, SHM_RDONLY);
    MP = (int *)shmat(shmid_MP, NULL, 0);
    
    if (MB == (int *)-1 || MP == (int *)-1) {
        perror("BP: shmat");
        exit(1);
    }
    
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    
    print_board();
    
    char ack = 'B';
    if (write(CP_pfd_write, &ack, 1) != 1) {
        perror("BP: write ack");
        shmdt(MB);
        shmdt(MP);
        exit(1);
    }
    
    while (1) {
        pause();
        
        if (print_requested) {
            print_requested = 0;
            print_board();
            
            if (write(CP_pfd_write, &ack, 1) != 1) {
                break;
            }
        }
    }
    
    shmdt(MB);
    shmdt(MP);
    close(CP_pfd_write);
    
    return 0;
}