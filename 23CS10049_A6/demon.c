#include <stdio.h>
#include <stdlib.h>
#include "house.h"
#include "cond.c"

house_t *H;
int shmid_H;
cond_t CV;
const char *state_str[] = {"EMPTY", "D_INSIDE", "N_INSIDE"};

void sigint_handler(int sig){
    (void)sig;
    shmdt(H);
    exit(0);
}

void demon_loop(int demon_id){
    srand(time(NULL) ^ (getpid() << 16)); // to create randomness in demons

    while(1){
        // mutex at wait
        cond_lock(CV);
        H->demon_wait_count++;
        printf("Demon %d arrives (D_CNT = %d, N_CNT = %d, state = %s)\n",demon_id, H->demon_count, H->nomad_count, state_str[H->state]);

        // if(H->state == NOMAD_INSIDE) 
        while(H->state == NOMAD_INSIDE)
        {
            printf("Demon %d waits (DW_CNT=%d)\n", demon_id, H->demon_wait_count);
            cond_wait(CV);
        }

        // mutex at entering
        H->demon_wait_count--;
        H->demon_count++;

        if (H->demon_count == 1) {
            H->state = DEMON_INSIDE;
            printf("Demon %d enters [house empty] (D_CNT=%d, N_CNT=%d, state = %s)\n", demon_id, H->demon_count, H->nomad_count, state_str[H->state]);
        } else {
            printf("Demon %d enters [other demons present] (D_CNT=%d, N_CNT=%d, state = %s)\n", demon_id, H->demon_count, H->nomad_count, state_str[H->state]);
        }

        cond_unlock(CV);

        // Spend time in the house (0.5 to 1 second)
        usleep((rand() % 500000) + 500000);

        cond_lock(CV);
        H->demon_count--;
        printf("Demon %d leaves (D_CNT=%d, N_CNT=%d, state = %s)\n", demon_id, H->demon_count, H->nomad_count, state_str[H->state]);

        if(H->demon_count == 0) {
            H->state = EMPTY;
            // we need to check if nomad is waiting then only send the signal else we have positive sem
            // and demon/nomad can bypass cond_wait
            if(H->nomad_wait_count>0) cond_signal(CV);
        }

        cond_unlock(CV);
        // Wander outside (1 to 5 seconds)
        usleep((rand() % 4000000) + 1000000);
        
    }
}

int main(int argc , char *argv[]){

    signal(SIGINT, sigint_handler);

    int n = 10;
    if(argc==2){
        n = atoi(argv[1]);
    }else if(argc>2){
        printf("Usage ./demon <n-demons>");
        exit(1);
    }else{}

    // generate keys
    key_t token1 = ftok("manager.c", TOK_MTX);
    key_t token2 = ftok("manager.c", TOK_CND);
    key_t token3 = ftok("manager.c", TOK_SHM);

    // we arent doing cond_create because it creates a new unique memory
    // we are fetching what is already created by the Manager
    CV.semid = semget(token1, 2, 0666);
    CV.shmid = shmget(token2, sizeof(int), 0666);

    shmid_H = shmget(token3, sizeof(house_t), 0666);
    H = (house_t *)shmat(shmid_H, NULL, 0);

    for (int i = 0; i < n; i++) {
        if (fork() == 0) {
            demon_loop(i);
            exit(0);
        }
    }
    // parent process will wait for all child to terminate and deattach
    while(wait(NULL) > 0); // busy wait on parent 
    shmdt(H);
    return 0;
}