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

#define MAX_USER_INPUT 10000
#define MAX_STATES 100
#define MAX_ALPHABET 26
#define TRANSITION 101
#define QUIT 102

int s;      // alphabet size
int n;      // number of states

int fd[MAX_STATES + 1][2];
int og_stdin, og_stdout;

typedef struct {
    int is_final;
    int transition[MAX_ALPHABET];
} DFAstates;

DFAstates states[MAX_STATES];

/* ================= SIGINT HANDLER ================= */

void terminate_processes_handler(int sig) {
#ifdef _VERBOSE
    dup2(og_stdout, STDOUT_FILENO);
    printf("\n      +++ Coordinator going to terminate all state processes\n");
    fflush(stdout);
#endif

    for (int i = 0; i < n; i++) {
        dup2(fd[i + 1][1], STDOUT_FILENO);
        printf("%d\n", QUIT);
        fflush(stdout);
    }

    dup2(og_stdout, STDOUT_FILENO);
    for (int i = 0; i < n; i++) {
        wait(NULL);
    }

#ifdef _VERBOSE
    printf("        +++ Coordinator: Bye\n");
#else
    printf("\n +++ Coordinator: Bye\n");
#endif
    fflush(stdout);
    exit(0);
}

/* ================= STATE PROCESS ================= */

void state_loop(int state_num) {

    int command;
    char symbol;
    int next_state;

    int is_final;
    int transition[MAX_ALPHABET];

    signal(SIGINT, SIG_IGN);

    dup2(fd[state_num + 1][0], STDIN_FILENO);

    scanf("%d", &is_final);
    for (int i = 0; i < s; i++) {
        scanf("%d", &transition[i]);
    }

    while (1) {

        scanf("%d", &command);

        if (command == QUIT) {
#ifdef _VERBOSE
            dup2(og_stdout, STDOUT_FILENO);
            printf("        +++ State %d going to quit\n", state_num);
            fflush(stdout);
#endif
            exit(0);
        }

        if (command == TRANSITION) {

            dup2(fd[0][1], STDOUT_FILENO);
            printf("%d\n", state_num);
            fflush(stdout);

            dup2(fd[state_num + 1][0], STDIN_FILENO);
            scanf(" %c", &symbol);

            if (symbol == '$') {
                dup2(og_stdout, STDOUT_FILENO);
                if (is_final) printf(" ACCEPT\n");
                else printf(" REJECT\n");
                fflush(stdout);

                dup2(fd[0][1], STDOUT_FILENO);
                printf("%d\n", state_num);
                fflush(stdout);
                continue;
            }

            int idx = symbol - 'a';
            if (idx < 0 || idx >= s) {
                dup2(og_stdout, STDOUT_FILENO);
                printf(" INVALID INPUT SYMBOL: %c\n", symbol);
                fflush(stdout);

                dup2(fd[0][1], STDOUT_FILENO);
                printf("%d\n", state_num);
                fflush(stdout);
                continue;
            }

            next_state = transition[idx];

            dup2(og_stdout, STDOUT_FILENO);
            printf(" -- %c --> %d", symbol, next_state);
            fflush(stdout);

            dup2(fd[next_state + 1][1], STDOUT_FILENO);
            printf("%d\n", TRANSITION);
            fflush(stdout);
        }
    }
}

/* ================= CONTROLLER ================= */

void user_loop() {

    char input[MAX_USER_INPUT];
    int current_state;

#ifdef _VERBOSE
    printf(" +++ Coordinator: Going to user loop\n");
    fflush(stdout);
#endif

    while (1) {

        dup2(og_stdin, STDIN_FILENO);
        dup2(og_stdout, STDOUT_FILENO);
        printf("Enter next String: ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;

        printf("0");
        fflush(stdout);

        dup2(fd[1][1], STDOUT_FILENO);
        printf("%d\n", TRANSITION);
        fflush(stdout);

        dup2(fd[0][0], STDIN_FILENO);
        scanf("%d", &current_state);

        for (int i = 0; input[i] != '\0'; i++) {
            dup2(fd[current_state + 1][1], STDOUT_FILENO);
            printf("%c\n", input[i]);
            fflush(stdout);

            dup2(fd[0][0], STDIN_FILENO);
            scanf("%d", &current_state);
        }

        dup2(fd[current_state + 1][1], STDOUT_FILENO);
        printf("$\n");
        fflush(stdout);

        dup2(fd[0][0], STDIN_FILENO);
        scanf("%d", &current_state);
    }
}

/* ================= MAIN ================= */

int main(int argc, char *argv[]) {

    char *filename = (argc == 1) ? "dfa.txt" : argv[1];
    FILE *fp;

    og_stdin = dup(STDIN_FILENO);
    og_stdout = dup(STDOUT_FILENO);

    fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        exit(1);
    }

    fscanf(fp, "%d", &s);
    fscanf(fp, "%d", &n);

    for (int i = 0; i < n; i++) {
        int id;
        char f;
        fscanf(fp, "%d %c", &id, &f);
        states[id].is_final = (f == 'F');
        for (int j = 0; j < s; j++) {
            fscanf(fp, "%d", &states[id].transition[j]);
        }
    }
    fclose(fp);

    for (int i = 0; i <= n; i++) pipe(fd[i]);

    for (int i = 0; i < n; i++) {
        if (fork() == 0) {
            state_loop(i);
            exit(0);
        }
    }

    sleep(1);

    for (int i = 0; i < n; i++) {
        dup2(fd[i + 1][1], STDOUT_FILENO);
        printf("%d\n", states[i].is_final);
        for (int j = 0; j < s; j++) printf("%d ", states[i].transition[j]);
        printf("\n");
        fflush(stdout);

#ifdef _VERBOSE
        dup2(og_stdout, STDOUT_FILENO);
        printf(" +++ %s state %d created\n",
               states[i].is_final ? "Final" : "Non-final", i);
        fflush(stdout);
#endif
    }

    dup2(og_stdout, STDOUT_FILENO);
    printf(" +++ Coordinator: %d state processes are created\n", n);
    fflush(stdout);

    signal(SIGINT, terminate_processes_handler);
    user_loop();
    terminate_processes_handler(SIGINT);
}
