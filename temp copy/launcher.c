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

void WAIT_Z(int semid, int semnum) {
    struct sembuf sb = {semnum, 0, 0};
    while (semop(semid, &sb, 1) == -1 && errno == EINTR);
}

int main() {
    FILE *fp = fopen("bursts.txt", "r");
    if (!fp) { perror("Failed to open bursts.txt"); return 1; }

    int shmid = shmget(SHM_KEY, sizeof(SharedMem), 0666);
    SharedMem *shm = (SharedMem *)shmat(shmid, NULL, 0);
    int semid = semget(SEM_KEY, 4, 0666);

    int arrival, priority, count = 0;
    int cpu[11], io[10];

    printf("[0] Launcher Ready\n");

    while (fscanf(fp, "%d", &arrival) != EOF && arrival != -1) {
        fscanf(fp, "%d", &priority);
        
        // FIX: Read interleaved CPU and IO bursts correctly
        for (int i = 0; i < 10; i++) {
            fscanf(fp, "%d", &cpu[i]);
            fscanf(fp, "%d", &io[i]);
        }
        fscanf(fp, "%d", &cpu[10]); // The 11th CPU burst

        while (1) {
            P(semid, SEM_T);
            int current_t = shm->t_info.t;
            V(semid, SEM_T);
            
            if (current_t >= arrival) break;
            usleep(DELTA);
            WAIT_Z(semid, SEM_SYNC);
        }

        if (fork() == 0) {
            char args[26][16];
            sprintf(args[0], "./process");
            sprintf(args[1], "%d", count);
            sprintf(args[2], "%d", arrival);
            sprintf(args[3], "%d", priority);
            
            // Pass them to the process sequentially so process.c doesn't need to change
            for (int i = 0; i < 11; i++) sprintf(args[4 + i], "%d", cpu[i]);
            for (int i = 0; i < 10; i++) sprintf(args[15 + i], "%d", io[i]);
            
            char *argv[26];
            for (int i = 0; i < 25; i++) argv[i] = args[i];
            argv[25] = NULL;

            execv("./process", argv);
            perror("execv failed");
            exit(1);
        }
        count++;
    }
    fclose(fp);

    for (int i = 0; i < count; i++) {
        wait(NULL);
    }

    printf("[Launcher] All user processes exited\n");
    kill(shm->t_info.timer_pid, SIGINT);
    kill(shm->t_info.manager_pid, SIGINT);

    return 0;
}