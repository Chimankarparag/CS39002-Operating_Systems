#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <errno.h>

/* -------- Global Configuration -------- */

#define MAX_TASKS 100
#define SHM_TOKEN 0x1234
#define SEM_TOKEN 0x5678

#define TICK_US 25000
#define SYNC_WAIT_US 5000

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

/* -------- Semaphore Utilities -------- */

void sem_wait_op(int semid, int idx) {
    struct sembuf op = { idx, -1, 0 };
    while (semop(semid, &op, 1) == -1 && errno == EINTR);
}

void sem_signal_op(int semid, int idx) {
    struct sembuf op = { idx, 1, 0 };
    while (semop(semid, &op, 1) == -1 && errno == EINTR);
}

void sem_wait_zero(int semid, int idx) {
    struct sembuf op = { idx, 0, 0 };
    while (semop(semid, &op, 1) == -1 && errno == EINTR);
}

int main() {

    FILE *input = fopen("bursts.txt", "r");
    if (!input) {
        perror("Unable to open bursts.txt");
        return 1;
    }

    int shmid = shmget(SHM_TOKEN, sizeof(SharedRegion), 0666);
    SharedRegion *shared = (SharedRegion *)shmat(shmid, NULL, 0);
    int semid = semget(SEM_TOKEN, 4, 0666);

    int arrival_time, prio;
    int total_spawned = 0;
    int cpu_burst[11], io_burst[10];

    printf("[0] Launcher initialized\n");

    while (fscanf(input, "%d", &arrival_time) != EOF && arrival_time != -1) {

        fscanf(input, "%d", &prio);

        /* Read alternating CPU/IO burst durations */
        for (int i = 0; i < 10; i++) {
            fscanf(input, "%d", &cpu_burst[i]);
            fscanf(input, "%d", &io_burst[i]);
        }
        fscanf(input, "%d", &cpu_burst[10]);

        /* Wait until global simulation time reaches arrival time */
        while (1) {
            sem_wait_op(semid, SEM_CLOCK);
            int current_time = shared->timer.time_tick;
            sem_signal_op(semid, SEM_CLOCK);

            if (current_time >= arrival_time)
                break;

            usleep(TICK_US);
            sem_wait_zero(semid, SEM_BARRIER);
        }

        if (fork() == 0) {

            char arg_buffer[26][16];
            char *argv_exec[26];

            sprintf(arg_buffer[0], "./process");
            sprintf(arg_buffer[1], "%d", total_spawned);
            sprintf(arg_buffer[2], "%d", arrival_time);
            sprintf(arg_buffer[3], "%d", prio);

            for (int i = 0; i < 11; i++)
                sprintf(arg_buffer[4 + i], "%d", cpu_burst[i]);

            for (int i = 0; i < 10; i++)
                sprintf(arg_buffer[15 + i], "%d", io_burst[i]);

            for (int i = 0; i < 25; i++)
                argv_exec[i] = arg_buffer[i];

            argv_exec[25] = NULL;

            execv("./process", argv_exec);
            perror("execv error");
            exit(1);
        }

        total_spawned++;
    }

    fclose(input);

    for (int i = 0; i < total_spawned; i++)
        wait(NULL);

    printf("[Launcher] All processes completed\n");

    kill(shared->timer.timer_pid, SIGINT);
    kill(shared->timer.manager_pid, SIGINT);

    return 0;
}