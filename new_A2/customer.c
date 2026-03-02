// 23CS10049 Parag Mahadeo Chimankar

#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

int cust_id;
int bookings_done = 0;
int waiting_for_response = 0;
pid_t agent_pid;

void handler_sigusr1(int sig){
    if (waiting_for_response) {
        bookings_done++;
        printf("            Customer %d: Booking %d successful\n", cust_id, bookings_done);
        fflush(stdout);
        waiting_for_response = 0;
        return;
    }

    if (bookings_done >= 2) {
        kill(agent_pid, SIGUSR2);
        printf("            Customer %d leaves the booking system\n", cust_id);
        fflush(stdout);
        exit(0);
    }

    if (bookings_done == 1) {
        int decide = rand() % 2;
        if (!decide) {
            kill(agent_pid, SIGUSR2);
            printf("            Customer %d leaves the booking system\n", cust_id);
            fflush(stdout);
            exit(0);
        }
    }

    int r = (rand() % 4) + 1;
    FILE *rf = fopen("request.txt", "w");
    if (rf) {
        fprintf(rf, "%d %d\n", cust_id, r);
        fclose(rf);
    } else {
        kill(agent_pid, SIGUSR2);
        printf("            Customer %d leaves the booking system\n", cust_id);
        fflush(stdout);
        exit(1);
    }

    printf("            Customer %d: Request for %d tickets\n", cust_id, r);
    fflush(stdout);
    
    kill(agent_pid, SIGUSR1);
    waiting_for_response = 1;
}

void handler_sigusr2(int sig){
    if (waiting_for_response) {
        printf("            Customer %d: Booking %d failed\n", cust_id, bookings_done + 1);
        fflush(stdout);
        waiting_for_response = 0;
        return;
    }
}

int main(int argc, char *argv[]){
    if (argc != 2) return 1;
    cust_id = atoi(argv[1]);
    agent_pid = getppid();
    srand((unsigned int)(time(NULL) ^ getpid()));

    printf("            Customer %d joins the booking system\n", cust_id);
    fflush(stdout);

    signal(SIGUSR1, handler_sigusr1);
    signal(SIGUSR2, handler_sigusr2);

    // to signal the parent that customer is ready
    kill(agent_pid, SIGCONT);
    // but since we are using sleep(1) in parent so didnt write SIGCONT handler in agent. signal is ignored

    while (1) pause();

    return 0;
}