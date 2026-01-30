// 23CS10049
// Parag Mahadeo Chimankar
// rundfa.c

/*
  (keeps original overview and constraints)
*/

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

    // coordinator to print to stdout(terminal)
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

/* helper: read one line from stdin (which will be redirected to a pipe fd) */
int read_line_from_stdin(char *buf, size_t bufsz) {
    if (fgets(buf, (int)bufsz, stdin) == NULL) return -1;
    size_t L = strlen(buf);
    if (L && buf[L-1] == '\n') buf[L-1] = '\0';
    return 0;
}

void state_loop(int state_num){
    // state gets the command from the controller/other states
    int command;
    int next_state;

    char transition_sym = 0;
    int is_final = 0;
    int transition[MAX_ALPHABET];

    char linebuf[128];

    // ignore SIGINT
    signal(SIGINT, SIG_IGN);

    // redirect this child's stdin to its pipe read end
    if (dup2(fd[state_num + 1][0], STDIN_FILENO) == -1) {
        perror("dup2 failed in child");
        exit(1);
    }

    /* read state information from coordinator (line-based) */
    if (read_line_from_stdin(linebuf, sizeof(linebuf)) == -1) exit(1);
    if (sscanf(linebuf, "%d", &is_final) != 1) exit(1);

    if (read_line_from_stdin(linebuf, sizeof(linebuf)) == -1) exit(1);
    {
        char *p = linebuf;
        for (int i = 0; i < s; i++) {
            int t;
            if (sscanf(p, "%d", &t) != 1) exit(1);
            transition[i] = t;
            // advance p past parsed integer
            while (*p && *p != ' ') p++;
            while (*p == ' ') p++;
        }
    }

    while (1) {
        /* wait for command line */
        if (read_line_from_stdin(linebuf, sizeof(linebuf)) == -1) {
            // parent might have closed pipe; exit cleanly
            exit(0);
        }
        if (sscanf(linebuf, "%d", &command) != 1) {
            // malformed command, ignore and continue
            continue;
        }

        if (command == QUIT) {
            #ifdef _VERBOSE
                dup2(og_stdout, STDOUT_FILENO);
                printf("        +++ State %d going to quit\n", state_num);
                fflush(stdout);
            #endif
            exit(0);
        }

        if (command == TRANSITION) {
            /* inform the coordinator about its current state */
            dup2(fd[0][1], STDOUT_FILENO);
            printf("%d\n", state_num);
            fflush(stdout);

            /* read next symbol line (line-based) */
            if (read_line_from_stdin(linebuf, sizeof(linebuf)) == -1) {
                // EOF => exit
                exit(0);
            }
            if (sscanf(linebuf, " %c", &transition_sym) != 1) {
                // malformed; print invalid symbol message
                dup2(og_stdout, STDOUT_FILENO);
                printf(" INVALID INPUT SYMBOL: (none)\n");
                fflush(stdout);
                continue;
            }

            /* check for final symbol */
            if (transition_sym == '$') {
                /* let coordinator know (send current state back) */
                dup2(fd[0][1], STDOUT_FILENO);
                printf("%d\n", state_num);
                fflush(stdout);
                continue;
            }

            /* validate symbol */
            int valid = transition_sym - 'a';
            if (valid < 0 || valid >= s) {
                dup2(og_stdout, STDOUT_FILENO);
                printf(" INVALID INPUT SYMBOL: %c\n", transition_sym);
                fflush(stdout);
                continue;
            }

            next_state = transition[valid];

            dup2(og_stdout, STDOUT_FILENO); // print to terminal
            printf(" -- %c --> %d", transition_sym, next_state);
            fflush(stdout);

            /* send TRANSITION command to next state */
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
        if (input[0] == '\0') {
            printf("\n");
            continue;
        }

        /* print starting state number */
        printf("%d", 0);
        fflush(stdout);

        /* send TRANSITION command to state 0 */
        dup2(fd[1][1], STDOUT_FILENO);
        printf("%d\n", TRANSITION);
        fflush(stdout);

        invalid_input = 0;

        for (marker = 0; input[marker] != '\0' && !invalid_input; marker++) {
            /* read current_state from coordinator pipe fd[0] (line-based) */
            dup2(fd[0][0], STDIN_FILENO);
            {
                char linebuf[64];
                if (fgets(linebuf, sizeof(linebuf), stdin) == NULL) {
                    invalid_input = 1;
                    break;
                }
                if (sscanf(linebuf, "%d", &current_state) != 1) {
                    invalid_input = 1;
                    break;
                }
            }

            /* send the character to the current state's pipe (line) */
            dup2(fd[current_state + 1][1], STDOUT_FILENO);
            printf("%c\n", input[marker]);
            fflush(stdout);

            int symbol_idx = input[marker] - 'a';
            if (symbol_idx < 0 || symbol_idx >= s) {
                invalid_input = 1;
            }
        }

        if (!invalid_input) {
            /* get current_state before sending $ */
            dup2(fd[0][0], STDIN_FILENO);
            {
                char linebuf[64];
                if (fgets(linebuf, sizeof(linebuf), stdin) == NULL) {
                    // treat as reject if we can't get it
                } else {
                    sscanf(linebuf, "%d", &current_state);
                }
            }

            /* send end-of-string marker */
            dup2(fd[current_state + 1][1], STDOUT_FILENO);
            printf("$\n");
            fflush(stdout);
            
            // read final state
            dup2(fd[0][0], STDIN_FILENO);
            scanf("%d", &current_state);

            // print result ONLY here
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

    if (argc == 1) {
        filename = "dfa.txt";
    } else {
        filename = argv[1];
    }
    // save original stdio
    og_stdin = dup(STDIN_FILENO);
    og_stdout = dup(STDOUT_FILENO);

    // open the dfa input file
    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        exit(1);
    }

    // read the dfa file
    if (fscanf(fp, "%d", &s) != 1) { fprintf(stderr, "bad dfa file\n"); exit(1); }
    if (fscanf(fp, "%d", &n) != 1) { fprintf(stderr, "bad dfa file\n"); exit(1); }
    for (i = 0; i < n; i++) {
        if (fscanf(fp, "%d %c", &state_num, &final_marker) != 2) { fprintf(stderr, "bad dfa file\n"); exit(1); }
        states[state_num].is_final = (final_marker == 'F') ? 1 : 0;
        for (j = 0; j < s; j++) {
            if (fscanf(fp, "%d", &states[state_num].transition[j]) != 1) { fprintf(stderr, "bad dfa file\n"); exit(1); }
        }
    }
    fclose(fp);

    for (i = 0; i <= n; i++) {
        if (pipe(fd[i]) == -1) {
            perror("pipe");
            exit(1);
        }
    }

    // create child processes
    for (i = 0; i < n; i++) {
        pid = fork();

        if (pid == -1) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            // child: close unrelated fds? not strictly necessary here
            state_loop(i);
            exit(0);
        }
    }

    sleep(1); // parents waits for 1 sec for child to be ready at read

    // send the state info (two lines per state): first line is final flag, second line is transitions
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
    // if user_loop ends without signal , terminate
    terminate_processes_handler(SIGINT);

    return 0;
}
