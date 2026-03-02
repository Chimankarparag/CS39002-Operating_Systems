// 23CS10049 Parag Mahadeo Chimankar

#define _POSIX_C_SOURCE 200112L 
// this is some standard version, since i am running it on macOS, kept it for uniformity across all platforms
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

#define MAXCOUNT 1024
int tickets;
int customers;
int qcap = 1024;
int queue[MAXCOUNT];
int qhead = 0, qtail = -1;

pid_t pid_of_id[MAXCOUNT];
int alive_of_id[MAXCOUNT];

// Queue implementation

void enqueue(int cust_id){
    qtail = (qtail + 1) % qcap;
    queue[qtail] = cust_id;
}

int dequeue(){
    if (qhead == (qtail + 1) % qcap) return -1;
    int cust_id = queue[qhead];
    qhead = (qhead + 1) % qcap;
    return cust_id;
}

void print_queue(){
    printf("\nAgent: Queue = (");
    int i = qhead;
    while (i != (qtail + 1) % qcap) {
        printf(" %d", queue[i]);
        i = (i + 1) % qcap;
    }
    printf(" ) Available = %d\n", tickets);
    fflush(stdout);
}


int pid_to_id(pid_t p){
    for(int i=1;i<=customers;i++) if (pid_of_id[i] == p) return i;
    return -1;
}

// terminate remaining customers 

void kill_remaining_and_exit(){
    int killedIds[1024], kcount=0;
    int i = qhead;
    while (i != (qtail + 1) % qcap) {
        int cust_id = queue[i];
        if (alive_of_id[cust_id]) {
            killedIds[kcount++] = cust_id;
        }
        i = (i + 1) % qcap;
    }

    if (kcount > 0) {
        printf("\nAgent terminates customers");
        for(int j=0;j<kcount;j++){
            printf(" %d", killedIds[j]);
            kill(pid_of_id[killedIds[j]], SIGKILL);
        }
        printf("\n");
        for(int j=0;j<kcount;j++){
            waitpid(pid_of_id[killedIds[j]], NULL, 0);
            alive_of_id[killedIds[j]] = 0;
        }
    }
    printf("\nAgent: Booking session over (no more tickets available)\n");
    exit(0);
}

// signal handler
void agent_handler(int sig, siginfo_t *info, void *ucontext){
    pid_t sender = info->si_pid;
    int id = pid_to_id(sender);
    if (id == -1) return;

    if (sig == SIGUSR2) {
        waitpid(sender, NULL, WNOHANG);
        alive_of_id[id] = 0;

        int anyAlive = 0;
        for(int i=1;i<=customers;i++) if (alive_of_id[i]) { anyAlive = 1; break; }
        int qempty = (qhead == (qtail + 1) % qcap);
        if (!anyAlive || qempty) {
            printf("\nAgent: Booking session over (no more customers available)\n");
            exit(0);
        }
        print_queue();
        int next = dequeue();
        if (next != -1) {
            kill(pid_of_id[next], SIGUSR1);
        }
        return;
    }

    if (sig == SIGUSR1) {
        FILE *request_file = fopen("request.txt", "r");
        if (!request_file) {
            enqueue(id);
            print_queue();
            int nxt = dequeue();
            if (nxt != -1) kill(pid_of_id[nxt], SIGUSR1);
            return;
        }

        int request_id = -1, request_req = -1;
        fscanf(request_file, "%d %d", &request_id, &request_req);
        fclose(request_file);

        if (request_req <= tickets) {
            tickets -= request_req;
            kill(sender, SIGUSR1);
        } else {
            kill(sender, SIGUSR2);
        }

        usleep(10000); // wait for customer response

        if (tickets <= 0) {
            kill_remaining_and_exit();
            return;
        }

        // debug : bring this lines after u sleep to print the last remaining customers in queue before it terminates
        if (alive_of_id[id]) enqueue(id);

        int q_empty = (qhead == (qtail + 1) % qcap);
        if (q_empty) {
            printf("\nAgent: Booking session over (no more customers available)\n");
            exit(0);
        }

        print_queue();
        
        int next = dequeue();
        if (next != -1) 
            kill(pid_of_id[next], SIGUSR1);

    }
}


int main(int argc, char *argv[]){
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <initial_tickets> <n_customers>\n", argv[0]);
        return 1;
    }
    tickets = atoi(argv[1]);
    customers = atoi(argv[2]);
    if (customers <= 0) return 1;

    srand((unsigned int)(time(NULL) ^ getpid()));
    //generating random sequence of customers
    int perm[MAXCOUNT];
    for(int i=1;i<=customers;i++) perm[i] = i;
    for(int i=customers;i>1;i--){
        int j = (rand() % i) + 1;
        int tmp = perm[i]; perm[i] = perm[j]; perm[j] = tmp;
    }

    struct sigaction sa;
    sa.sa_sigaction = agent_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    // newer sigaction method instead of signal(SIGUSR#, signalhandler);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    for(int id=1; id<=customers; id++){
        pid_t p = fork();
        if (p == 0) {
            char idstr[16];
            snprintf(idstr, sizeof(idstr), "%d", id);
            execl("./customer", "./customer", idstr, NULL);
            perror("execl customer failed");
            exit(1);
        } else {
            pid_of_id[id] = p;
            alive_of_id[id] = 1;
        }
    }

    for(int i=1;i<=customers;i++){
        enqueue(perm[i]);
    }

    sleep(1);
    // sleeping for 1 second to avoid race condition of customer initialistion and parent process
    print_queue();

    int first = dequeue();
    if (first != -1) {
        kill(pid_of_id[first], SIGUSR1);
    }

    while(1) pause();

    return 0;
}