#include <stdio.h>
#include <stdlib.h>
#include "house.h"
#include "cond.c"

int main(){
    // generate keys
    key_t token1 = ftok("manager.c", TOK_MTX);
    key_t token2 = ftok("manager.c", TOK_CND);
    key_t token3 = ftok("manager.c", TOK_SHM);

    // use the cond api func created
    cond_t CV = cond_create(token1,token2);
    cond_init(CV);

    // shared memory house
    int shmid_H = shmget(token3, sizeof(house_t), IPC_CREAT | 0666);
    house_t *H = (house_t*)shmat(shmid_H, NULL, 0);

    // init house to zero
    H->state = EMPTY;
    H->demon_count = 0;
    H->nomad_count = 0;
    H->demon_wait_count = 0;
    H->nomad_wait_count = 0;

    // deattach
    shmdt(H);

    // simulation run
    printf("Manager running. Press Enter to terminate...\n");
    getchar();

    // clean up IPC resources 
    shmctl(shmid_H, IPC_RMID, NULL);
    cond_destroy(CV);
    
    printf("Resources successfully cleaned up.\n");
    return 0;




}