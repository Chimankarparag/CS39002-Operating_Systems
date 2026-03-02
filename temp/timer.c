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

#define DELTA 25000       
#define DELTA_DELAY 5000  

#define SEM_RQ 0
#define SEM_PCB 1
#define SEM_T 2
#define SEM_SYNC 3

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

void P(int semid, int semnum) {
    struct sembuf sb = {semnum, -1, 0};
    while (semop(semid, &sb, 1) == -1 && errno == EINTR);
}

void V(int semid, int semnum) {
    struct sembuf sb = {semnum, 1, 0};
    while (semop(semid, &sb, 1) == -1 && errno == EINTR);
}

void sigint_handler(int sig) {
    printf("\n[Timer] Terminating.\n");
    exit(0);
}

int main() {
    signal(SIGINT, sigint_handler);

    key_t shm_key = ftok(".", 'M');
    key_t sem_key = ftok(".", 'S');

    int shmid = shmget(shm_key, sizeof(SharedMem), 0666);
    SharedMem *shm = (SharedMem *)shmat(shmid, NULL, 0);
    int semid = semget(sem_key, 4, 0666);

    shm->t_info.timer_pid = getpid();

    P(semid, SEM_SYNC);
    shm->t_info.t = 0;
    V(semid, SEM_SYNC);

    while (1) {
        usleep(DELTA + DELTA_DELAY);
        
        P(semid, SEM_T);
        shm->t_info.t++;
        int current_time = shm->t_info.t;
        int running = shm->t_info.current_running;
        int next_int = shm->t_info.next_interrupt;
        
        if (running != -1 && next_int == current_time) {
            P(semid, SEM_PCB);
            pid_t p_pid = shm->pcb[running].pid;
            V(semid, SEM_PCB);
            kill(p_pid, SIGUSR1);
        }
        V(semid, SEM_T);

        P(semid, SEM_SYNC); 
        usleep(DELTA_DELAY);
        V(semid, SEM_SYNC); 
    }

    return 0;
}