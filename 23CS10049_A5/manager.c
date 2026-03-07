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

// Process states
#define STATE_READY 0
#define STATE_RUNNING 1
#define STATE_IO 2
#define STATE_EXITED 3

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

// union semun {
//     int val;
//     struct semid_ds *buf;
//     unsigned short *array;
// };

// setting up shared memory and semaphores ID in Global scope for signal handlers
int shmid, semid;

void sigint_handler(int sig) {
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    //debug statement
    printf("\n[Manager] Cleaned up shared memory and semaphores. Exiting.\n");
    exit(0);
}

int main(){
    signal(SIGINT, sigint_handler);

    key_t shm_key = ftok(".", 'M'); // for memory
    key_t sem_key = ftok(".", 'S'); // for semaphore

    /*
    Alternatively we can also use Global Variable Predefined keys, but it may fail if 
    other shared memory already uses the key
    */
    if (shm_key == -1 || sem_key == -1) {
        perror("ftok failed");
        exit(1);
    }

    shmid = shmget(shm_key,sizeof(SharedMem),IPC_CREAT | 0666);
    SharedMem *shm = (SharedMem *) shmat(shmid, NULL, 0);

    semid = semget(sem_key, 4, IPC_CREAT | 0666);
    union semun sem_arg; // This was predefined in my MacOs 
    // otherwise uncomment lines 58-62 to use user-defined semun
    // if already defined keep it commented as it may create ambiguity

    sem_arg.val = 1;
    semctl(semid, SEM_RQ, SETVAL, sem_arg);
    semctl(semid, SEM_PCB, SETVAL, sem_arg);
    semctl(semid, SEM_T, SETVAL, sem_arg);
    sem_arg.val = 0;
    semctl(semid, SEM_SYNC, SETVAL, sem_arg);

    shm->rq.front = 0;
    shm->rq.back = 0;
    shm->t_info.t = 0;
    shm->t_info.current_running = -1;
    shm->t_info.next_interrupt = -1;
    shm->t_info.is_started = 0;
    shm->t_info.manager_pid = getpid();

    // deattach so that manager process doesnt interfere in shared mem
    shmdt(shm);
    printf("[Manager] Resources created. Waiting for termination...\n");

    while (1) {
        pause();
    }
    return 0;

}