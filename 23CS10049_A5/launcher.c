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
#define EXECV_PARM_LEN 16 // for storing execv parameters

// Time
#define DELTA 50000     // bigger delta range 25ms to 1s
#define DELTA_DELAY 10000 // smaller delta range 5ms to 10ms

// Semaphore indices
#define SEM_RQ 0
#define SEM_PCB 1
#define SEM_T 2
#define SEM_SYNC 3

typedef struct {
    int q[MAX_PROC + 1];
    int front;
    int back;
} RQ_type;

typedef struct {
    int id;
    pid_t pid;
    int priority;
    int state;
}PCB_type;

typedef struct {
    int t;
    int current_running;
    int next_interrupt;
    pid_t manager_pid;
    pid_t timer_pid;
    int is_started;
} T_type;

typedef struct {
    RQ_type rq;
    PCB_type pcb[MAX_PROC];
    T_type t_info;
} SharedMem;

/*
- uncomment if not pre-defined in system 

struct sembuf {
    unsigned short sem_num;  
    short          sem_op;   
    short          sem_flg;  
};
*/

void wait_sem(int semid, int semnum) {
    struct sembuf sb = {semnum, -1, 0};
    while (semop(semid, &sb, 1) == -1 && errno == EINTR);
}

void signal_sem(int semid, int semnum) {
    struct sembuf sb = {semnum, 1, 0};
    while (semop(semid, &sb, 1) == -1 && errno == EINTR);
}

void wait_zero(int semid, int semnum) {
    struct sembuf sb = {semnum, 0, 0};
    while (semop(semid, &sb, 1) == -1) {
        if (errno != EINTR) {
            perror("semop wait-for-zero failed");
            exit(1);
        }
    }
}

int main() {

    FILE *fp = fopen("bursts.txt", "r");
    if (!fp) { perror("Failed to open bursts.txt"); return 1; }

    key_t shm_key = ftok(".", 'M');
    key_t sem_key = ftok(".", 'S');

    int shmid = shmget(shm_key, sizeof(SharedMem), 0666);
    SharedMem *shm = (SharedMem *)shmat(shmid, NULL, 0);
    int semid = semget(sem_key, 4, 0666);

    int arrival, priority, cnt = 0;
    // burst ( predefined as it was given in question)
    int cpu[11], io[10];

    printf("[0] Launcher Ready\n");

    while (fscanf(fp, "%d", &arrival) != EOF && arrival != -1) {
        fscanf(fp, "%d", &priority);
        
        for (int i = 0; i < 10; i++) {
            fscanf(fp, "%d", &cpu[i]);
            fscanf(fp, "%d", &io[i]);
        }
        fscanf(fp, "%d", &cpu[10]); 

        while (1) {
            //mutex for arrival time and process setup sync
            wait_sem(semid, SEM_T);
                int current_t = shm->t_info.t;
            signal_sem(semid, SEM_T);
            
            if (current_t >= arrival) break;
            usleep(DELTA);
            wait_zero(semid, SEM_SYNC);
        }

        if (fork() == 0) {
            char args[26][EXECV_PARM_LEN];
            sprintf(args[0], "./process");
            sprintf(args[1], "%d", cnt);
            sprintf(args[2], "%d", arrival);
            sprintf(args[3], "%d", priority);
            
            for (int i = 0; i < 11; i++) sprintf(args[4 + i], "%d", cpu[i]);
            for (int i = 0; i < 10; i++) sprintf(args[15 + i], "%d", io[i]);
            
            char *argv[26];
            for (int i = 0; i < 25; i++) argv[i] = args[i];
            argv[25] = NULL;

            execv("./process", argv);
            perror("execv failed");
            exit(1);
        }
        cnt++;
    }
    fclose(fp);

    for (int i = 0; i < cnt; i++) {
        wait(NULL);
    }

    printf("[Launcher] All processes exited\n");
    kill(shm->t_info.timer_pid, SIGINT);
    kill(shm->t_info.manager_pid, SIGINT);

    return 0;
}