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
#define DELTA 40000     // bigger delta range 25ms to 1s
#define DELTA_DELAY 7000 // smaller delta range 5ms to 10ms

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

volatile sig_atomic_t interrupted = 0;
void sigusr1_handler(int sig) {
    interrupted = 1;
}

SharedMem *shm;
int semid;
int id, local_t;


void schedule_next(int mode) {
    wait_sem(semid, SEM_T);
    wait_sem(semid, SEM_RQ);
    wait_sem(semid, SEM_PCB);

    if (mode == 2) { 
        shm->rq.q[shm->rq.back] = id;
        shm->rq.back = (shm->rq.back + 1) % (MAX_PROC + 1);
        printf("[%d] Process %d: Interrupted\n", local_t, id);
    } else if (mode == 0) { 
        shm->rq.q[shm->rq.back] = id;
        shm->rq.back = (shm->rq.back + 1) % (MAX_PROC + 1);
        if (shm->t_info.current_running != -1) {
            signal_sem(semid, SEM_PCB);
            signal_sem(semid, SEM_RQ);
            signal_sem(semid, SEM_T);
            return;
        }
    }

    if (shm->rq.front != shm->rq.back) { 
        int next_id = shm->rq.q[shm->rq.front];
        shm->rq.front = (shm->rq.front + 1) % (MAX_PROC + 1);
        
        shm->t_info.current_running = next_id;
        shm->pcb[next_id].state = STATE_RUNNING;
        
        int q = (shm->pcb[next_id].priority == 0) ? 10 : (shm->pcb[next_id].priority == 1 ? 5 : 2);
        shm->t_info.next_interrupt = local_t + q;
        
        if (mode == 2) {
            printf("[%d] Process %d: Context switch to process %d with next interrupt time = %d\n", local_t, id, next_id, shm->t_info.next_interrupt);
        } else {
            printf("[%d] Process %d: Going from READY to RUNNING with next interrupt time = %d\n", local_t, next_id, shm->t_info.next_interrupt);
        }
    } else {
        shm->t_info.current_running = -1;
        shm->t_info.next_interrupt = -1;
        printf("[%d] CPU goes idle\n", local_t);
    }

    signal_sem(semid, SEM_PCB);
    signal_sem(semid, SEM_RQ);
    signal_sem(semid, SEM_T);
}

int main(int argc, char *argv[]) {
    signal(SIGUSR1, sigusr1_handler);

    key_t shm_key = ftok(".", 'M');
    key_t sem_key = ftok(".", 'S');

    int shmid = shmget(shm_key, sizeof(SharedMem), 0666);
    shm = (SharedMem *)shmat(shmid, NULL, 0);
    semid = semget(sem_key, 4, 0666);

    // Parse command line arguments
    id = atoi(argv[1]);
    local_t = atoi(argv[2]);
    int priority = atoi(argv[3]);
    int cpu[11], io[10];
    for (int i = 0; i < 11; i++) cpu[i] = atoi(argv[4 + i]);
    for (int i = 0; i < 10; i++) io[i] = atoi(argv[15 + i]);

    // mutex for process Arrival
    wait_sem(semid, SEM_PCB);
        shm->pcb[id].id = id;
        shm->pcb[id].pid = getpid();
        shm->pcb[id].priority = priority;
        shm->pcb[id].state = STATE_READY;
    signal_sem(semid, SEM_PCB);

    printf("[%d] Process %d: Arrival with priority = %d\n", local_t, id, priority);

    //
    if (id == 0) signal_sem(semid, SEM_SYNC); 
    schedule_next(0);

    int cb_idx = 0, io_idx = 0;

    while (1) {
        usleep(DELTA);
        wait_zero(semid, SEM_SYNC);
        
        wait_sem(semid, SEM_T);
            local_t = shm->t_info.t;
        signal_sem(semid, SEM_T);

        wait_sem(semid, SEM_PCB);
            int current_state = shm->pcb[id].state;
        signal_sem(semid, SEM_PCB);

        if (current_state == STATE_RUNNING) {
            // Assign time quantum based on priority
            int my_q = (priority == 0) ? 10 : (priority == 1 ? 5 : 2);
            
            wait_sem(semid, SEM_T);
                int dispatch_time = shm->t_info.next_interrupt - my_q;
            signal_sem(semid, SEM_T);

            if (dispatch_time < local_t) {
                cpu[cb_idx]--;
                if (cpu[cb_idx] == 0) {
                    interrupted = 0; 
                    printf("[%d] Process %d: CPU burst %d complete\n", local_t, id, cb_idx);
                    if (cb_idx == 10) {
                        wait_sem(semid, SEM_PCB);
                            shm->pcb[id].state = STATE_EXITED;
                        signal_sem(semid, SEM_PCB);
                        printf("\t\t\t[%d] Process %d: Exiting\n", local_t, id);
                        schedule_next(1);
                        break;
                    } else {
                        wait_sem(semid, SEM_PCB);
                        shm->pcb[id].state = STATE_IO;
                        signal_sem(semid, SEM_PCB);
                        schedule_next(1);
                    }
                }
            }
        } else if (current_state == STATE_IO) {
            io[io_idx]--;
            if (io[io_idx] == 0) {
                printf("[%d] Process %d: IO burst %d complete\n", local_t, id, io_idx);
                cb_idx++; io_idx++;

                wait_sem(semid, SEM_PCB);
                    shm->pcb[id].state = STATE_READY;
                signal_sem(semid, SEM_PCB);
                schedule_next(0);
            }
        }
        
        if (interrupted && current_state == STATE_RUNNING && cpu[cb_idx] > 0) {
            interrupted = 0;
            
            usleep(2000); 

            wait_sem(semid, SEM_PCB);
                    shm->pcb[id].state = STATE_READY;
            signal_sem(semid, SEM_PCB);
            schedule_next(2);
        }
    }
    return 0;
}