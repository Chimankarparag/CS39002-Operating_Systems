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

// Details for Semaphore templates at bottom 

void wait_sem(int semid, int semnum) {
    struct sembuf sb = {semnum, -1, 0};
    while (semop(semid, &sb, 1) == -1 && errno == EINTR);
}

void signal_sem(int semid, int semnum) {
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
    
    // initial mutex for sync when all binaries created and run
    wait_sem(semid, SEM_SYNC);
    shm->t_info.t =0;
    signal_sem(semid, SEM_SYNC);

    while(1){
        usleep(DELTA+DELTA_DELAY);
        // mutex for using timer
        wait_sem(semid, SEM_T);

        shm->t_info.t++;
        int current_time = shm->t_info.t;
        int running = shm->t_info.current_running;
        int next_int = shm->t_info.next_interrupt;

        if (running != -1 && next_int == current_time) {
            // At the end of the time quantum q of the currently running process, it
            // checks whether that process is still running. If so, it sends SIGUSR1 to that process.
            // mutex for Processes 
            wait_sem(semid, SEM_PCB);
                pid_t p_pid = shm->pcb[running].pid;
            signal_sem(semid, SEM_PCB);

            kill(p_pid, SIGUSR1);
        }

        signal_sem(semid, SEM_T);

        // mutex for sync
        wait_sem(semid,SEM_SYNC);
            usleep(DELTA_DELAY);
        signal_sem(semid, SEM_SYNC);

    }
    return 0;
}


/* 
TEMPLATES

struct sembuf {
    unsigned short sem_num;  // The index of the semaphore in the array (e.g., 0, 1, 2)
    short          sem_op;   // The operation to perform (-1 for wait, 1 for signal, 0 for wait-for-zero)
    short          sem_flg;  // Flags (usually 0, or IPC_NOWAIT, or SEM_UNDO)
};

void wait_sem(int semid, int sem_index) {
    struct sembuf sb;
    sb.sem_num = sem_index; // Which semaphore in the set to use
    sb.sem_op = -1;         // Decrement by 1 (Wait/Lock)
    sb.sem_flg = 0;         // Default blocking behavior

    // The while loop ensures that if a signal interrupts the wait, it automatically retries.
    while (semop(semid, &sb, 1) == -1) {
        if (errno != EINTR) {
            perror("semop wait failed");
            exit(1);
        }
    }
}

void signal_sem(int semid, int sem_index) {
    struct sembuf sb;
    sb.sem_num = sem_index; // Which semaphore in the set to use
    sb.sem_op = 1;          // Increment by 1 (Signal/Unlock)
    sb.sem_flg = 0;         // Default behavior

    while (semop(semid, &sb, 1) == -1) {
        if (errno != EINTR) {
            perror("semop signal failed");
            exit(1);
        }
    }
}

void wait_for_zero(int semid, int sem_index) {
    struct sembuf sb;
    sb.sem_num = sem_index; // Which semaphore in the set to use
    sb.sem_op = 0;          // Block until the semaphore value is exactly 0
    sb.sem_flg = 0;         // Default blocking behavior

    while (semop(semid, &sb, 1) == -1) {
        if (errno != EINTR) {
            perror("semop wait-for-zero failed");
            exit(1);
        }
    }
}

*/