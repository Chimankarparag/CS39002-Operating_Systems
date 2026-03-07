#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <errno.h>

/* ---------------- System Configuration ---------------- */

#define MAX_TASKS 100
#define SHM_TOKEN 0x1234
#define SEM_TOKEN 0x5678

#define TICK_US 25000

/* Semaphore indices */
#define SEM_QUEUE   0
#define SEM_TABLE   1
#define SEM_CLOCK   2
#define SEM_BARRIER 3

/* Process states */
#define STATE_READY    0
#define STATE_RUNNING  1
#define STATE_WAITING  2
#define STATE_FINISHED 3

/* ---------------- Shared Structures ---------------- */

typedef struct {
    int buffer[MAX_TASKS + 1];
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
    int running_id;
    int interrupt_time;
    pid_t manager_pid;
    pid_t timer_pid;
} ClockData;

typedef struct {
    ReadyQueue rq;
    PCB pcb[MAX_TASKS];
    ClockData clock;
} SharedRegion;

/* ---------------- Semaphore Wrappers ---------------- */

void sem_wait_op(int semid, int index) {
    struct sembuf op = { index, -1, 0 };
    while (semop(semid, &op, 1) == -1 && errno == EINTR);
}

void sem_signal_op(int semid, int index) {
    struct sembuf op = { index, 1, 0 };
    while (semop(semid, &op, 1) == -1 && errno == EINTR);
}

void sem_wait_zero(int semid, int index) {
    struct sembuf op = { index, 0, 0 };
    while (semop(semid, &op, 1) == -1 && errno == EINTR);
}

/* ---------------- Global Context ---------------- */

volatile sig_atomic_t preempt_request = 0;

void handle_preemption(int sig) {
    preempt_request = 1;
}

SharedRegion *shared;
int semid;
int proc_id;
int local_time;

/* ---------------- Scheduler Logic ---------------- */

void scheduler(int cause) {

    sem_wait_op(semid, SEM_CLOCK);
    sem_wait_op(semid, SEM_QUEUE);
    sem_wait_op(semid, SEM_TABLE);

    /* Reinsert into ready queue if needed */
    if (cause == 2 || cause == 0) {
        shared->rq.buffer[shared->rq.tail] = proc_id;
        shared->rq.tail = (shared->rq.tail + 1) % (MAX_TASKS + 1);

        if (cause == 0 && shared->clock.running_id != -1) {
            sem_signal_op(semid, SEM_TABLE);
            sem_signal_op(semid, SEM_QUEUE);
            sem_signal_op(semid, SEM_CLOCK);
            return;
        }

        if (cause == 2) {
            printf("[%d] Process %d preempted\n",
                   local_time, proc_id);
        }
    }

    /* Select next runnable process */
    if (shared->rq.head != shared->rq.tail) {

        int next = shared->rq.buffer[shared->rq.head];
        shared->rq.head =
            (shared->rq.head + 1) % (MAX_TASKS + 1);

        shared->clock.running_id = next;
        shared->pcb[next].state = STATE_RUNNING;

        int quantum =
            (shared->pcb[next].priority == 0) ? 10 :
            (shared->pcb[next].priority == 1) ? 5 : 2;

        shared->clock.interrupt_time =
            local_time + quantum;

        printf("[%d] Dispatching process %d (interrupt at %d)\n",
               local_time,
               next,
               shared->clock.interrupt_time);
    }
    else {
        shared->clock.running_id = -1;
        shared->clock.interrupt_time = -1;
        printf("[%d] CPU idle\n", local_time);
    }

    sem_signal_op(semid, SEM_TABLE);
    sem_signal_op(semid, SEM_QUEUE);
    sem_signal_op(semid, SEM_CLOCK);
}

/* ---------------- Main ---------------- */

int main(int argc, char *argv[]) {

    signal(SIGUSR1, handle_preemption);

    int shmid =
        shmget(SHM_TOKEN,
               sizeof(SharedRegion),
               0666);

    shared =
        (SharedRegion *)shmat(shmid, NULL, 0);

    semid = semget(SEM_TOKEN, 4, 0666);

    /* Parse command-line arguments */
    proc_id   = atoi(argv[1]);
    local_time = atoi(argv[2]);
    int priority = atoi(argv[3]);

    int cpu_burst[11];
    int io_burst[10];

    for (int i = 0; i < 11; i++)
        cpu_burst[i] = atoi(argv[4 + i]);

    for (int i = 0; i < 10; i++)
        io_burst[i] = atoi(argv[15 + i]);

    /* Initialize PCB entry */
    sem_wait_op(semid, SEM_TABLE);

    shared->pcb[proc_id].id = proc_id;
    shared->pcb[proc_id].pid = getpid();
    shared->pcb[proc_id].priority = priority;
    shared->pcb[proc_id].state = STATE_READY;

    sem_signal_op(semid, SEM_TABLE);

    printf("[%d] Process %d arrived (priority=%d)\n",
           local_time, proc_id, priority);

    if (proc_id == 0)
        sem_signal_op(semid, SEM_BARRIER);

    scheduler(0);

    int cpu_index = 0;
    int io_index  = 0;

    /* Execution cycle */
    while (1) {

        usleep(TICK_US);
        sem_wait_zero(semid, SEM_BARRIER);

        sem_wait_op(semid, SEM_CLOCK);
        local_time = shared->clock.time_tick;
        sem_signal_op(semid, SEM_CLOCK);

        sem_wait_op(semid, SEM_TABLE);
        int state = shared->pcb[proc_id].state;
        sem_signal_op(semid, SEM_TABLE);

        /* CPU burst handling */
        if (state == STATE_RUNNING) {

            int quantum =
                (priority == 0) ? 10 :
                (priority == 1) ? 5 : 2;

            sem_wait_op(semid, SEM_CLOCK);
            int dispatch_start =
                shared->clock.interrupt_time - quantum;
            sem_signal_op(semid, SEM_CLOCK);

            if (dispatch_start < local_time) {

                cpu_burst[cpu_index]--;

                if (cpu_burst[cpu_index] == 0) {

                    preempt_request = 0;

                    printf("[%d] Process %d completed CPU burst %d\n",
                           local_time,
                           proc_id,
                           cpu_index);

                    if (cpu_index == 10) {

                        sem_wait_op(semid, SEM_TABLE);
                        shared->pcb[proc_id].state =
                            STATE_FINISHED;
                        sem_signal_op(semid, SEM_TABLE);

                        printf("[%d] Process %d exiting\n",
                               local_time, proc_id);

                        scheduler(1);
                        break;
                    }
                    else {

                        sem_wait_op(semid, SEM_TABLE);
                        shared->pcb[proc_id].state =
                            STATE_WAITING;
                        sem_signal_op(semid, SEM_TABLE);

                        scheduler(1);
                    }
                }
            }
        }

        /* IO burst handling */
        else if (state == STATE_WAITING) {

            io_burst[io_index]--;

            if (io_burst[io_index] == 0) {

                printf("[%d] Process %d completed IO burst %d\n",
                       local_time,
                       proc_id,
                       io_index);

                cpu_index++;
                io_index++;

                sem_wait_op(semid, SEM_TABLE);
                shared->pcb[proc_id].state = STATE_READY;
                sem_signal_op(semid, SEM_TABLE);

                scheduler(0);
            }
        }

        /* Handle preemption */
        if (preempt_request &&
            state == STATE_RUNNING &&
            cpu_burst[cpu_index] > 0) {

            preempt_request = 0;

            /* Small delay to resolve tie conditions */
            usleep(2000);

            sem_wait_op(semid, SEM_TABLE);
            shared->pcb[proc_id].state = STATE_READY;
            sem_signal_op(semid, SEM_TABLE);

            scheduler(2);
        }
    }

    return 0;
}