#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>

/* -------- Configuration -------- */

#define MAX_TASKS 100
#define SHM_TOKEN 0x1234
#define SEM_TOKEN 0x5678

#define SEM_QUEUE   0
#define SEM_TABLE   1
#define SEM_CLOCK   2
#define SEM_BARRIER 3

typedef struct {
    int items[MAX_TASKS + 1];
    int head;
    int tail;
} ReadyQueue;

typedef struct {
    int id;
    pid_t pid;
    int priority;
    int state;
} PCB;

typedef struct {
    int time_tick;
    int active_process;
    int interrupt_at;
    pid_t manager_pid;
    pid_t timer_pid;
} TimerInfo;

typedef struct {
    ReadyQueue rq;
    PCB pcb[MAX_TASKS];
    TimerInfo timer;
} SharedRegion;

int global_shmid, global_semid;

void cleanup(int sig) {
    shmctl(global_shmid, IPC_RMID, NULL);
    semctl(global_semid, 0, IPC_RMID);
    printf("\n[Manager] IPC resources cleaned.\n");
    exit(0);
}

int main() {

    signal(SIGINT, cleanup);

    global_shmid = shmget(SHM_TOKEN, sizeof(SharedRegion),
                          IPC_CREAT | 0666);

    SharedRegion *shared =
        (SharedRegion *)shmat(global_shmid, NULL, 0);

    global_semid = semget(SEM_TOKEN, 4, IPC_CREAT | 0666);

    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } sem_cfg;

    sem_cfg.val = 1;
    semctl(global_semid, SEM_QUEUE, SETVAL, sem_cfg);
    semctl(global_semid, SEM_TABLE, SETVAL, sem_cfg);
    semctl(global_semid, SEM_CLOCK, SETVAL, sem_cfg);

    sem_cfg.val = 0;
    semctl(global_semid, SEM_BARRIER, SETVAL, sem_cfg);

    /* Initialize shared structures */
    shared->rq.head = 0;
    shared->rq.tail = 0;

    shared->timer.time_tick = 0;
    shared->timer.active_process = -1;
    shared->timer.interrupt_at = -1;
    shared->timer.manager_pid = getpid();

    shmdt(shared);

    printf("[Manager] Shared memory and semaphores initialized.\n");

    while (1)
        pause();
}