// 23CS10049
// Parag Mahadeo Chimankar
// rundfa.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_USER_INPUT 10000
#define MAX_STATES 100
#define MAX_ALPHABET 26
#define TRANSITION 101
#define QUIT 102

int s;  // alphabet size
int n;  // number of states

int fd[MAX_STATES + 1][2];
int og_stdin, og_stdout;

typedef struct {
    int is_final;
    int transition[MAX_ALPHABET];
} DFAstates;

DFAstates states[MAX_STATES];

void terminate_processes_handler(int signal){
    #ifdef _VERBOSE
        dup2(og_stdout, STDOUT_FILENO);
        printf("\n      +++ Coordinator going to terminate all state processes\n");
        fflush(stdout);
    #endif

    // send QUIT message to all child processes
    for (int i = 0; i < n; i++) {
        dup2(fd[i + 1][1], STDOUT_FILENO);
        printf("%d\n", QUIT);
        fflush(stdout);
    }

    dup2(og_stdout, STDOUT_FILENO);

    for (int i = 0; i < n; i++) {
        wait(NULL);
    }

    #ifndef _VERBOSE
        printf(" +++ Coordinator: All state processes terminated. Bye.\n");
    #else
        printf("        +++ Coordinator: Bye\n");
    #endif
    fflush(stdout);
    exit(0);
}

void state_loop(int state_num){
    int command;
    int next_state;
    char transition_sym = 0;
    int is_final = 0;
    int transition[MAX_ALPHABET];

    signal(SIGINT, SIG_IGN);

    // redirect this child's stdin to its pipe read end
    if (dup2(fd[state_num + 1][0], STDIN_FILENO) == -1) {
        exit(1);
    }

    // Read initialization
    if (scanf("%d", &is_final) != 1) exit(1);
    for (int i = 0; i < s; i++) {
        if (scanf("%d", &transition[i]) != 1) exit(1);
    }

    while (1) {
        // Wait for command
        if (scanf("%d", &command) != 1) exit(0);

        if (command == QUIT) {
            #ifdef _VERBOSE
                dup2(og_stdout, STDOUT_FILENO);
                printf("        +++ State %d going to quit\n", state_num);
                fflush(stdout);
            #endif
            exit(0);
        }

        if (command == TRANSITION) {
            // 1. Acknowledge activation (Send ID to coordinator)
            dup2(fd[0][1], STDOUT_FILENO);
            printf("%d\n", state_num);
            fflush(stdout);

            // 2. Read symbol
            dup2(fd[state_num + 1][0], STDIN_FILENO);
            // Use " %c" to handle potential whitespace, though parent handles newlines now
            if (scanf(" %c", &transition_sym) != 1) exit(0);

            // 3. Handle End-of-Input
            if (transition_sym == '$') {
                // Acknowledge receipt of $ (Coordinator waits for this before printing result)
                dup2(fd[0][1], STDOUT_FILENO);
                printf("%d\n", state_num);
                fflush(stdout);
                continue;
            }

            // 4. Validate Symbol
            int valid = transition_sym - 'a';
            if (valid < 0 || valid >= s) {
                dup2(og_stdout, STDOUT_FILENO);
                printf(" INVALID INPUT SYMBOL: %c\n", transition_sym);
                fflush(stdout);
                continue;
            }

            // 5. Transition
            next_state = transition[valid];

            dup2(og_stdout, STDOUT_FILENO); 
            printf(" -- %c --> %d", transition_sym, next_state);
            fflush(stdout);

            // 6. Wake up next state
            dup2(fd[next_state + 1][1], STDOUT_FILENO);
            printf("%d\n", TRANSITION);
            fflush(stdout);
        }
    }
}

void user_loop(){
    char input[MAX_USER_INPUT];
    int marker;
    int current_state;
    int invalid_input;

    #ifdef _VERBOSE
    printf(" +++ Coordinator: Going to user loop\n");
    fflush(stdout);
    #endif

    while (1) {
        dup2(og_stdin, STDIN_FILENO);
        dup2(og_stdout, STDOUT_FILENO);
        printf("Enter next String: ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\n")] = 0;
        
        // FIX 1: Removed empty string check. Empty strings are now processed.

        /* Print starting state visual */
        printf("%d", 0);
        fflush(stdout);

        /* 1. Send TRANSITION command to State 0 */
        dup2(fd[1][1], STDOUT_FILENO);
        printf("%d\n", TRANSITION);
        fflush(stdout);

        /* 2. Wait for State 0 to acknowledge it is active */
        // We MUST read this before entering the loop to ensure sync
        dup2(fd[0][0], STDIN_FILENO);
        if (scanf("%d", &current_state) != 1) break; 
        getchar(); // FIX 2: Consume the '\n' left by scanf in the pipe!

        invalid_input = 0;

        /* 3. Loop through characters */
        for (marker = 0; input[marker] != '\0'; marker++) {
            
            // Check validity BEFORE sending to avoid getting stuck
            int symbol_idx = input[marker] - 'a';
            if (symbol_idx < 0 || symbol_idx >= s) {
                // Send the invalid char to state so it prints the error message
                dup2(fd[current_state + 1][1], STDOUT_FILENO);
                printf("%c\n", input[marker]);
                fflush(stdout);
                
                invalid_input = 1;
                break; // Stop processing this string
            }

            // Send valid char to current state
            dup2(fd[current_state + 1][1], STDOUT_FILENO);
            printf("%c\n", input[marker]);
            fflush(stdout);

            // Wait for the NEXT state to acknowledge activation
            dup2(fd[0][0], STDIN_FILENO);
            scanf("%d", &current_state);
            getchar(); // FIX 2: Consume the '\n'
        }

        /* 4. Handle Result */
        if (!invalid_input) {
            // Send '$' to the LAST active state
            dup2(fd[current_state + 1][1], STDOUT_FILENO);
            printf("$\n");
            fflush(stdout);
            
            // Wait for state to confirm it received '$'
            dup2(fd[0][0], STDIN_FILENO);
            scanf("%d", &current_state);
            getchar(); // FIX 2: Consume the '\n'

            // Print Verdict
            dup2(og_stdout, STDOUT_FILENO);
            if (states[current_state].is_final) {
                printf(" ACCEPT\n");
            } else {
                printf(" REJECT\n");
            }
            fflush(stdout);
        }

        dup2(og_stdout, STDOUT_FILENO);
        printf("\n");
        fflush(stdout);
    }
}

int main(int argc, char *argv[]) {
    char *filename;
    FILE *fp;
    int i, j, state_num;
    char final_marker;
    pid_t pid;

    // Disable buffering on stdin to prevent mixed-IO issues
    setvbuf(stdin, NULL, _IONBF, 0);

    if (argc == 1) {
        filename = "dfa.txt";
    } else {
        filename = argv[1];
    }
    
    og_stdin = dup(STDIN_FILENO);
    og_stdout = dup(STDOUT_FILENO);

    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        exit(1);
    }

    if (fscanf(fp, "%d", &s) != 1) exit(1);
    if (fscanf(fp, "%d", &n) != 1) exit(1);
    for (i = 0; i < n; i++) {
        fscanf(fp, "%d %c", &state_num, &final_marker);
        states[state_num].is_final = (final_marker == 'F') ? 1 : 0;
        for (j = 0; j < s; j++) {
            fscanf(fp, "%d", &states[state_num].transition[j]);
        }
    }
    fclose(fp);

    for (i = 0; i <= n; i++) {
        if (pipe(fd[i]) == -1) {
            perror("pipe"); exit(1);
        }
    }

    for (i = 0; i < n; i++) {
        pid = fork();
        if (pid == -1) { perror("fork"); exit(1); }
        if (pid == 0) {
            state_loop(i);
            exit(0);
        }
    }

    sleep(1); 

    for (i = 0; i < n; i++) {
        dup2(fd[i + 1][1], STDOUT_FILENO);
        printf("%d\n", states[i].is_final);
        for (j = 0; j < s; j++) {
            printf("%d ", states[i].transition[j]);
        }
        printf("\n");
        fflush(stdout);

        #ifdef _VERBOSE
            dup2(og_stdout, STDOUT_FILENO);
            if (states[i].is_final) {
                printf(" +++ Final state %d created\n", i);
            } else {
                printf(" +++ Non-final state %d created\n", i);
            }
            fflush(stdout);
        #endif
    }

    dup2(og_stdout, STDOUT_FILENO);
    printf(" +++ Coordinator: %d state processes are created\n", n);
    fflush(stdout);

    signal(SIGINT, terminate_processes_handler);

    user_loop();
    terminate_processes_handler(SIGINT);

    return 0;
}