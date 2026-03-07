#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct{
    int semid; // for two semaphores mtx and cnd
    int shmid; // for shared int count
}cond_t;

#define MTX 0
#define CND 1

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

cond_t cond_create(key_t token1, key_t token2){
    cond_t CV;
    CV.semid = semget(token1, 2, IPC_CREAT|0666);
    CV.shmid = shmget(token2, sizeof(int), IPC_CREAT|0666);
    return CV;
}

void cond_init(cond_t CV){

    // init semaphore
    union semun arg;
    // if the environment/system already has semun declared keep lines 19-23 commented
    // else uncomment it 
    // (if system already has its declaration) -> keep the lines commented
    arg.val=1;
    semctl(CV.semid, MTX, SETVAL, arg);

    arg.val=0;
    semctl(CV.semid, CND, SETVAL, arg);

    // init shared memory count
    int *count =(int *)shmat(CV.shmid, NULL, 0);
    *count = 0;
    shmdt(count);
}

/*
struct sembuf {
    ushort sem_num; // sub-semaphore
    short sem_op; // execute the sem-op value
    shoet sem_flag;
}
*/

void cond_lock(cond_t CV){
    struct sembuf sop = {MTX, -1, 0};
    semop(CV.semid, &sop, 1);
}

void cond_unlock(cond_t CV){
    struct sembuf sop = {MTX, 1, 0};
    semop(CV.semid, &sop, 1);
}

void cond_wait(cond_t CV){
    int *count =(int *)shmat(CV.shmid, NULL, 0);
    (*count)++;
    shmdt(count);
    cond_unlock(CV); // Release mtx
    
    sleep(1); // Simulate non-atomicity

    struct sembuf sop = {CND, -1, 0};
    semop(CV.semid, &sop, 1); // Wait on cnd
    cond_lock(CV); // Re-acquire mtx
    count = (int*)shmat(CV.shmid, NULL, 0);
    (*count)--;
    shmdt(count);
    // User must unlock mtx after this (explained in commments at the end of code)
}

void cond_signal(cond_t CV){
    int *count = (int *)shmat(CV.shmid, NULL, 0);
    if(*count>0){
        struct sembuf sop = {CND, 1, 0};
        semop(CV.semid, &sop, 1); // Wait on cnd
    }
    shmdt(count);
    // User must unlock mtx after this
}

// Wake all waiting processes
void cond_broadcast(cond_t CV) {
    int *count = (int*)shmat(CV.shmid, NULL, 0);
    int n = *count;
    shmdt(count);

    if (n > 0) {
        struct sembuf sop = {CND, (short)n, 0};
        semop(CV.semid, &sop, 1);
    }
    // User must unlock mtx after this
}

// Destroy condition variable
void cond_destroy(cond_t CV) {
    shmctl(CV.shmid, IPC_RMID, NULL);
    semctl(CV.semid, 0, IPC_RMID);
}

/*

USER code will have: 

T1
----
m_lock(&M);

if(C)
    cond_wait(&C, &M);

m_unlock(&M);

 T2
----
m_lock(&M);

if(!C)
    cond_signal(&C);

m_unlock(&M);


*/