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

#define BOARD_SIZE 101    
#define MAX_PLAYERS 26  

int *MB;
int *MP;
int num_players;
int CP_pfd_write;
pid_t bp_pid;
pid_t *player_pids;
int current_player=0;

volatile sig_atomic_t move_requested = 0; 
volatile sig_atomic_t my_turn = 0;

int roll_dice() {
    return (rand() % 6) + 1;
}

void sigusr1_handler_pp(int sig){
    move_requested =1;
}

void sigusr1_handler_player(int sig) {
    my_turn = 1;
}
void sigusr2_handler_child(int sig) {
    shmdt(MB);
    shmdt(MP);
    exit(0);
}

void sigusr2_handler_pp(int sig){
    for(int i = 0;i<num_players;i++){
        if(player_pids[i]>0){
            kill(player_pids[i],SIGUSR2);
        }
    }
    for (int i = 0; i < num_players; i++) {
        if (player_pids[i] > 0) {
            int status;
            waitpid(player_pids[i], &status, 0);
            printf("Player %c terminated.\n", 'A' + i);
            fflush(stdout);
            //sleep(1); 
        }
    }

    shmdt(MB);
    shmdt(MP);
    close(CP_pfd_write);
    free(player_pids);

    printf("\nAll Players exited. Closing Players window...\n");
    fflush(stdout);
    sleep(3); 
    exit(0);
}

void make_move(int player_idx) {
    printf("******************************\n");
    char player_name = 'A' + player_idx;
    int pos = MP[player_idx];
    
    if (pos == 100) {
        return;
    }
    
    int total = 0;
    int six_counter =0;
    int throw;
    printf("Player %c: ",player_name);
    fflush(stdout);
    while (1) {
        throw = roll_dice();
        printf("%d",throw);
        fflush(stdout);
        
        if(throw == 6){
            six_counter++;            
            total+=throw;
            if(six_counter!=3){
                printf(" + ");
                fflush(stdout);
            }else{
                printf(" X ");
                total =0;
                six_counter=0;
                fflush(stdout);
            }

        }else{
            total+=throw;
            six_counter=0;
            break;
        }
    }
    printf("\n");
    fflush(stdout);
    int nextpos = pos + total;
    
    if (nextpos > 100) {
        printf("Move not permitted (Cannot go beyond 100)\n");
        fflush(stdout);
        return;
    }
    
    while (MB[nextpos] != 0) {
        int prev = nextpos;
        nextpos += MB[nextpos];
        
        if (MB[prev] > 0) {
            printf("Ladder at cell %d, Jump to %d\n", prev, nextpos);
            fflush(stdout);
        } else {
            printf("Snake at cell %d, Jump to %d\n", prev, nextpos);
            fflush(stdout);
        }
    }

    if (nextpos > 100 || nextpos < 0) {
        printf("Move not permitted (Cannot go beyond 100 or Less than 0 )\n");
        fflush(stdout);
        return;
    }

    for (int p = 0; p < num_players; p++) {
        if (p != player_idx && MP[p] == nextpos && nextpos != 100) {
            printf("Move not permitted (Cell already occupied by %c)\n", 
                   'A' + p);
            fflush(stdout);
            return;
        }
    }

    if(nextpos !=100) printf("Player %c moves to cell %d\n", player_name, nextpos);
    else printf("Player %c exits with rank = %d\n", player_name,num_players-MP[num_players]+1);

    
    fflush(stdout);
    
    // Update position
    MP[player_idx] = nextpos;
    
    if (nextpos == 100) {
        MP[num_players]--;
        kill(bp_pid, SIGUSR1);
        shmdt(MB);
        shmdt(MP);
        close(CP_pfd_write);
        exit(0);
    }
    return;
}

void player_main(int player_idx) {
    srand(time(NULL) ^ getpid());
    
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler_player; 
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = sigusr2_handler_child;
    sigaction(SIGUSR2, &sa, NULL);
    
    while (1) {
        pause(); 
        
        if (my_turn) {
            my_turn = 0;
            make_move(player_idx);
            kill(bp_pid, SIGUSR1);
        }
    }
    exit(0);
}


int main(int argc, char *argv[]) {
    if (argc != 4) { 
        fprintf(stderr, "Usage: %s <num_players> <pipe> <bp_pid>\n", argv[0]);
        exit(1);
    }

    num_players = atoi(argv[1]);
    CP_pfd_write = atoi(argv[2]);
    bp_pid = atoi(argv[3]);

    // Send PP's PID to CP via pipe
    pid_t pp_pid = getpid();
    if (write(CP_pfd_write, &pp_pid, sizeof(pid_t)) != sizeof(pid_t)) {
        perror("PP: write PID to pipe");
        exit(1);
    }

    // Attach to shared memory segments
    key_t key_MB = ftok("ludo.txt", 'B');
    key_t key_MP = ftok("ludo.txt", 'P');

    
    int shmid_MB = shmget(key_MB, BOARD_SIZE * sizeof(int), 0666);
    int shmid_MP = shmget(key_MP, (num_players + 1) * sizeof(int), 0666);
    
    if (shmid_MB == -1 || shmid_MP == -1) {
        perror("PP: shmget");
        exit(1);
    }
    
    MB = (int *)shmat(shmid_MB, NULL, SHM_RDONLY);
    MP = (int *)shmat(shmid_MP, NULL, 0);
    
    if (MB == (int *)-1 || MP == (int *)-1) {
        perror("PP: shmat");
        exit(1);
    }

    player_pids = (pid_t *)malloc(num_players * sizeof(pid_t));
    
    //init signal handlers for PP
    struct sigaction sa1, sa2;
    
    sa1.sa_handler = sigusr1_handler_pp;
    sigemptyset(&sa1.sa_mask);
    sa1.sa_flags = 0;
    sigaction(SIGUSR1, &sa1, NULL);
    
    sa2.sa_handler = sigusr2_handler_pp;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = 0;
    sigaction(SIGUSR2, &sa2, NULL);

    // forking n processes for n players
    for (int i = 0; i < num_players; i++) {
        pid_t pid = fork();
        
        if (pid == -1) {
            perror("fork player");
            exit(1);
        }
        
        if (pid == 0) {
            player_main(i);  
        }
        
        player_pids[i] = pid;
    }

    //printf("debug : Player processes created\n");
    // fflush(stdout);
    while (1) {
        pause();
        
        if (move_requested) {
            move_requested = 0;
            
            int attempts = 0;
            while (attempts < num_players) {
                if (MP[current_player] != 100) {
                    break;
                }
                current_player = (current_player + 1) % num_players;
                attempts++;
            }
            
            if (MP[num_players] == 0) {
                continue;
            }
            
            kill(player_pids[current_player], SIGUSR1);
            current_player = (current_player + 1) % num_players;
        }
    }
    
    return 0;

}