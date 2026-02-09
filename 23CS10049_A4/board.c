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

void print_board() {
    printf("\033[2J\033[H");  // Clear screen
    
    for (int row = 9; row >= 0; row--) {
        printf("|");
        if (row % 2 == 1) {
            for (int col = 0; col < 10; col++) {
                int cell = row * 10 + col + 1;
                printf("%3d", cell);
                
                if (MB[cell] > 0) printf(" L");
                else if (MB[cell] < 0) printf(" S");
                else printf("  ");
                
                printf("|");
            }
        } else {
            for (int col = 9; col >= 0; col--) {
                int cell = row * 10 + col + 1;
                printf("%3d", cell);
                
                if (MB[cell] > 0) printf(" L");
                else if (MB[cell] < 0) printf(" S");
                else printf("  ");
                
                printf("|");
            }
        }
        printf("\n");
    }

    printf("\n");
    for (int i = 0; i < num_players; i++) {
        if (MP[i] == 100) {
            printf("%c ", 'A' + i);
        }
    }
    printf("\n");
    
    for (int i = 0; i < num_players; i++) {
        if (MP[i] > 0 && MP[i] < 100) {
            printf("%c(%d) ", 'A' + i, MP[i]);
        } else if (MP[i] == 0) {
            printf("%c ", 'A' + i);
        }
    }
    printf("\n");
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
    
    key_t key_MB = ftok(".", 'B');
    key_t key_MP = ftok(".", 'P');
    
    int shmid_MB = shmget(key_MB, BOARD_SIZE * sizeof(int), 0666);
    int shmid_MP = shmget(key_MP, (num_players+ 1) * sizeof(int), 0666);
    
    if (shmid_MB == -1 || shmid_MP == -1) {
        perror("BP: shmget");
        exit(1);
    }
    
    MB = (int *)shmat(shmid_MB, NULL, 0);
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