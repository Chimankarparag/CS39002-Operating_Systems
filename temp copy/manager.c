#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define MAX_PROC 100
#define SHM_KEY 0x1234
#define SEM_KEY 0x5678

// Simulation Time Scales (in microseconds)
#define DELTA 25000       
#define DELTA_DELAY 5000  

// Semaphore Indices
#define SEM_RQ 0
#define SEM_PCB 1
#define SEM_T 2
#define SEM_SYNC 3

// Process States
#define STATE_READY 0
#define STATE_RUNNING 1
#define STATE_IO 2
#define STATE_EXITED 3

typedef struct {
    int queue[MAX_PROC + 1];
    int front;
    int rear;
} RQ_t;

typedef struct {
    int id;
    pid_t pid;
    int priority;
    int state; 
} PCB_t;

typedef struct {
    int t;
    int current_running;
    int next_interrupt;
    pid_t manager_pid;
    pid_t timer_pid;
} T_t;

typedef struct {
    RQ_t rq;
    PCB_t pcb[MAX_PROC];
    T_t t_info;
} SharedMem;

// union semun {
//     int val;
//     struct semid_ds *buf;
//     unsigned short *array;
// };

int shmid, semid;

void sigint_handler(int sig) {
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID, 0);
    printf("\n[Manager] IPC resources removed. Exiting.\n");
    exit(0);
}

int main() {
    signal(SIGINT, sigint_handler);

    shmid = shmget(SHM_KEY, sizeof(SharedMem), IPC_CREAT | 0666);
    SharedMem *shm = (SharedMem *)shmat(shmid, NULL, 0);

    semid = semget(SEM_KEY, 4, IPC_CREAT | 0666);
    union semun sem_arg;

    sem_arg.val = 1;
    semctl(semid, SEM_RQ, SETVAL, sem_arg);
    semctl(semid, SEM_PCB, SETVAL, sem_arg);
    semctl(semid, SEM_T, SETVAL, sem_arg);
    sem_arg.val = 0;
    semctl(semid, SEM_SYNC, SETVAL, sem_arg);

    shm->rq.front = 0;
    shm->rq.rear = 0;
    shm->t_info.t = 0;
    shm->t_info.current_running = -1;
    shm->t_info.next_interrupt = -1;
    shm->t_info.manager_pid = getpid();

    shmdt(shm);
    printf("[Manager] Resources created. Waiting for termination...\n");

    while (1) {
        pause();
    }
    return 0;
}