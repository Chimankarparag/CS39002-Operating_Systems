#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_STATES 100
#define MAX_ALPHABET 26
#define BUFFER_SIZE 256

int s, n;
int pipes[MAX_STATES + 1][2];  
int saved_stdin, saved_stdout; 
pid_t state_pids[MAX_STATES];

int delta[MAX_STATES][MAX_ALPHABET];
int is_final[MAX_STATES];

int my_state, my_is_final;
int my_delta[MAX_ALPHABET];

void coordinator_loop();
void state_loop();
void windup_handler(int sig);
void read_dfa_file(const char *filename);

int main(int argc, char *argv[]) {
    const char *filename = (argc > 1) ? argv[1] : "dfa.txt";
    read_dfa_file(filename);
    
    saved_stdin = dup(STDIN_FILENO);
    saved_stdout = dup(STDOUT_FILENO);
    
    for (int i = 0; i <= n; i++) {
        if (pipe(pipes[i]) == -1) exit(1);
    }
    
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            my_state = i;
            my_is_final = is_final[i];
            memcpy(my_delta, delta[i], sizeof(int) * s);
            signal(SIGINT, SIG_IGN);
            state_loop();
            exit(0);
        }
        state_pids[i] = pid;
        
#ifdef _VERBOSE
        dup2(saved_stdout, STDOUT_FILENO);
        printf("\t\t\t+++ %s state %d created\n", (is_final[i] ? "Final" : "Non-final"), i);
        fflush(stdout);
#endif
    }
    
    sleep(1);
    
    dup2(saved_stdout, STDOUT_FILENO);
    printf("\t\t\t+++ Coordinator: %d state processes are created\n", n);
#ifdef _VERBOSE
    printf("\t\t\t+++ Coordinator: Going to user loop\n");
#endif
    fflush(stdout);

    signal(SIGINT, windup_handler);
    coordinator_loop();
    
    return 0;
}

void read_dfa_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) exit(1);
    fscanf(fp, "%d %d", &s, &n);
    for (int i = 0; i < n; i++) {
        int st; char f;
        fscanf(fp, "%d %c", &st, &f);
        is_final[st] = (f == 'F');
        for (int j = 0; j < s; j++) fscanf(fp, "%d", &delta[st][j]);
    }
    fclose(fp);
}

void coordinator_loop() {
    char input[BUFFER_SIZE];
    while (1) {
        dup2(saved_stdin, STDIN_FILENO);
        dup2(saved_stdout, STDOUT_FILENO);
        printf("\nEnter next string: ");
        fflush(stdout);
        
        if (scanf("%s", input) == EOF) break;

        dup2(pipes[0][1], STDOUT_FILENO);
        printf("TRANSITION\n");
        fflush(stdout);

        int len = strlen(input);
        for (int i = 0; i <= len; i++) {
            int curr_state;
            dup2(pipes[n][0], STDIN_FILENO);
            if (scanf("%d", &curr_state) != 1) break;

            dup2(pipes[curr_state][1], STDOUT_FILENO);
            if (i == len) printf("EOI\n");
            else printf("%c\n", input[i]);
            fflush(stdout);

            if (i < len && (input[i] < 'a' || input[i] >= 'a' + s)) break;
            
            if (i == len) {
                // Wait for the state process to signal that it finished printing
                dup2(pipes[n][0], STDIN_FILENO);
                int sync;
                scanf("%d", &sync);
                break;
            }
        }
    }
}

void state_loop() {
    char cmd[BUFFER_SIZE];
    while (1) {
        dup2(pipes[my_state][0], STDIN_FILENO);
        if (scanf("%s", cmd) != 1) continue;
        
        if (strcmp(cmd, "QUIT") == 0) {
#ifdef _VERBOSE
            dup2(saved_stdout, STDOUT_FILENO);
            printf("\t\t\t+++ State %d going to quit\n", my_state);
            fflush(stdout);
#endif
            exit(0);
        } else if (strcmp(cmd, "TRANSITION") == 0) {
            dup2(pipes[n][1], STDOUT_FILENO);
            printf("%d\n", my_state);
            fflush(stdout);
            
            char sym[BUFFER_SIZE];
            dup2(pipes[my_state][0], STDIN_FILENO);
            scanf("%s", sym);
            
            dup2(saved_stdout, STDOUT_FILENO);
            if (strcmp(sym, "EOI") == 0) {
                // Print result on the same line as the trace
                printf("%d %s\n", my_state, my_is_final ? "ACCEPT" : "REJECT");
                fflush(stdout);
                
                // Signal Coordinator that the simulation for this string is done
                dup2(pipes[n][1], STDOUT_FILENO);
                printf("%d\n", my_state);
                fflush(stdout);
            } else {
                char c = sym[0];
                if (c < 'a' || c >= 'a' + s) {
                    printf("%d INVALID INPUT SYMBOL: %c\n", my_state, c);
                    fflush(stdout);
                    
                    // Signal Coordinator even on invalid input
                    dup2(pipes[n][1], STDOUT_FILENO);
                    printf("%d\n", my_state);
                    fflush(stdout);
                } else {
                    int next = my_delta[c - 'a'];
                    printf("%d -- %c --> ", my_state, c);
                    fflush(stdout);
                    
                    dup2(pipes[next][1], STDOUT_FILENO);
                    printf("TRANSITION\n");
                    fflush(stdout);
                }
            }
        }
    }
}

void windup_handler(int sig) {
#ifdef _VERBOSE
    dup2(saved_stdout, STDOUT_FILENO);
    printf("\n\t\t\t+++ Coordinator going to terminate all state processes\n");
    fflush(stdout);
#endif

    for (int i = 0; i < n; i++) {
        dup2(pipes[i][1], STDOUT_FILENO);
        printf("QUIT\n");
        fflush(stdout);
    }
    
    for (int i = 0; i < n; i++) waitpid(state_pids[i], NULL, 0);
    
    dup2(saved_stdout, STDOUT_FILENO);
    printf("\t\t+++ Coordinator: Bye\n");
    fflush(stdout);
    exit(0);
}