#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <errno.h>

/* -------- System Limits -------- */

#define MAX_TASKS 100
#define SHM_TOKEN 0x1234
#define SEM_TOKEN 0x5678

#define CLOCK_STEP_US 25000
#define SYNC_GAP_US   5000

/* -------- Semaphore Positions -------- */

#define SEM_QUEUE   0
#define SEM_TABLE   1
#define SEM_CLOCK   2
#define SEM_BARRIER 3

/* -------- Data Structures -------- */

typedef struct {
    int entries[MAX_TASKS + 1];
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

/* -------- Semaphore Helpers -------- */

void sem_wait_op(int semid, int index) {
    struct sembuf op = { index, -1, 0 };
    while (semop(semid, &op, 1) == -1 && errno == EINTR);
}

void sem_signal_op(int semid, int index) {
    struct sembuf op = { index, 1, 0 };
    while (semop(semid, &op, 1) == -1 && errno == EINTR);
}

void terminate_timer(int sig) {
    printf("\n[Timer] Shutting down.\n");
    exit(0);
}

int main() {
    signal(SIGINT, terminate_timer);

    int shmid = shmget(SHM_TOKEN, sizeof(SharedRegion), 0666);
    SharedRegion *shared = (SharedRegion *)shmat(shmid, NULL, 0);
    int semid = semget(SEM_TOKEN, 4, 0666);

    shared->timer.timer_pid = getpid();

    /* Initialize clock safely */
    sem_wait_op(semid, SEM_BARRIER);
    shared->timer.time_tick = 0;
    sem_signal_op(semid, SEM_BARRIER);

    while (1) {

        usleep(CLOCK_STEP_US + SYNC_GAP_US);

        /* Advance global clock and handle quantum expiration */
        sem_wait_op(semid, SEM_CLOCK);
        shared->timer.time_tick++;

        int now = shared->timer.time_tick;
        int running = shared->timer.active_process;
        int expiry = shared->timer.interrupt_at;

        if (running != -1 && expiry == now) {
            sem_wait_op(semid, SEM_TABLE);
            pid_t target = shared->pcb[running].pid;
            sem_signal_op(semid, SEM_TABLE);
            kill(target, SIGUSR1);
        }

        sem_signal_op(semid, SEM_CLOCK);

        /* Release barrier to synchronize all processes for next tick */
        sem_wait_op(semid, SEM_BARRIER);
        usleep(SYNC_GAP_US);
        sem_signal_op(semid, SEM_BARRIER);
    }
}