// 23CS10049 
// Parag Mahadeo Chimankar
// rundfa.c

/*
Overview of question

First make the dfa
there is a controller process and 'n' child processes
child processes represents the DFA states
controller can read the input string( one symbol at a time ) , Child cannot

user inputs the string 
controller -> S0 get start trigger ( transition message sent by controller ) via a Pipe Ps0 
State tells controller its info and ask controller for symbol
controller sends the symbol to child process, and child process sends another transition message to the next state Si via Pipe Psi

then Si informs controller about its corrent state , asks for symbol and controller sends the next symbol
continues...

Edge cases :
1. Invalid state transition ( check if transition for that symbol exists )
2. Encounter the End of String ( check if it is a final state )
3. Controller asks the user to end next input string

constraint:

1. Cannot use read and write system calls
2. Use printf and scanf only for stdio and use dup
3. On Ctrl+C SIGINT should be captured only by controller and ignored by childred, 
controller send QUIT message to all chilren and then children should terminate with a goodbye message,
once all the children are terminated, the controller terminates.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

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

// signal handling for SIGINT (Ctrl + C)
void terminate_processes_handler(int signal){
    #ifdef _VERBOSE
        dup2(og_stdout, STDOUT_FILENO);
        printf("\n      +++ Coordinator going to terminate all state processes\n");
        fflush(stdout);
    #endif

    // send QUIT message to all child processes
    for( int i = 0; i < n; i++){
        dup2(fd[i + 1][1], STDOUT_FILENO);
        printf("%d\n", QUIT);
        fflush(stdout);
    }
    
    // coordinator to print to stdout(terminal)
    dup2(og_stdout, STDOUT_FILENO);

    for( int i = 0; i < n; i++){
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
    // state gets the command from the controller/other states
    int command;
    int next_state;

    char transition_sym;
    int is_final;
    int transition[MAX_ALPHABET];

    // ignore SIGINT 
    signal(SIGINT, SIG_IGN);

    // read state information from coordinator 
    if (dup2(fd[state_num + 1][0], STDIN_FILENO) == -1) {
        perror("dup2 failed");
        exit(1);
    }

    scanf("%d", &is_final);
    for (int i = 0; i < s; i++) {
        if (scanf("%d", &transition[i]) != 1) exit(1);
    }

    while(1){

        scanf("%d", &command);

        if(command == QUIT){

            #ifdef _VERBOSE
                dup2(og_stdout, STDOUT_FILENO);
                printf("        +++ State %d going to quit\n", state_num);
                fflush(stdout);
            #endif

            exit(0);
        }
        if(command == TRANSITION){
            // inform the coordinator about its current state
            dup2(fd[0][1],STDOUT_FILENO);
            printf("%d\n", state_num);
            fflush(stdout);

            dup2(fd[state_num + 1][0], STDIN_FILENO);
            scanf(" %c", &transition_sym);

            // check for final state and final symbol
            if( transition_sym =='$'){

                dup2(fd[0][1], STDOUT_FILENO);
                printf ("%d\n", state_num);
                fflush(stdout);
                continue;
            }

            // check for valid
            int valid = transition_sym-'a';
            if(valid < 0 || valid >= s){
                dup2(og_stdout,STDOUT_FILENO);
                printf(" INVALID INPUT SYMBOL: %c\n", transition_sym);
                fflush(stdout);
                continue;
            }

            next_state = transition[valid];


            dup2(og_stdout, STDOUT_FILENO); // print to terminal 
            printf(" -- %c --> %d", transition_sym, next_state);
            fflush(stdout);

            // send TRANSITION command to next state
            dup2(fd[next_state + 1][1], STDOUT_FILENO);
            printf("%d\n",TRANSITION);
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

    while(1){
        dup2(og_stdin,STDIN_FILENO);
        dup2(og_stdout,STDOUT_FILENO);
        printf("Enter next String: ");
        fflush(stdout);

        if(fgets(input, sizeof(input), stdin)== NULL){
            break;
        }
        input[strcspn(input, "\n")] = 0;

        printf("%d",0);
        fflush(stdout);

        dup2(fd[1][1], STDOUT_FILENO);
        printf("%d\n", TRANSITION);
        fflush(stdout);

        // debug here , clean the stdin for remaining buffer of null or newline
        // tells coordinator that the state is ready
        dup2(fd[0][0], STDIN_FILENO);
        if (scanf("%d", &current_state) != 1) break; 
        getchar();

        invalid_input = 0;


        for (marker = 0; input[marker] != '\0'; marker++) {

            int symbol_idx = input[marker] - 'a';
            if (symbol_idx < 0 || symbol_idx >= s) {
                // Send the invalid char to state so it prints the error message
                dup2(fd[current_state + 1][1], STDOUT_FILENO);
                printf("%c\n", input[marker]);
                fflush(stdout);
                
                invalid_input = 1;
                break; // Stop processing this string
            }
            dup2(fd[current_state + 1][1], STDOUT_FILENO);
            printf("%c\n", input[marker]);
            fflush(stdout);
            
            // Wait for the NEXT state to acknowledge activation
            dup2(fd[0][0], STDIN_FILENO);
            scanf("%d", &current_state);
            getchar();

        }
        
        if (!invalid_input) {            
            dup2(fd[current_state + 1][1], STDOUT_FILENO);
            printf("$\n");
            fflush(stdout);

            dup2(fd[0][0], STDIN_FILENO);
            scanf("%d", &current_state);
            getchar();
        }

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

int main(int argc, char *argv[]) {
    char *filename;
    FILE *fp;
    int i, j, state_num;
    char final_marker;
    pid_t pid;

    // there was some excess buffer of '\0' and '\n' in stdin, to prevent it 
    setvbuf(stdin, NULL, _IONBF, 0);

    if( argc == 1){
        filename = "dfa.txt";
    }else{
        filename = argv[1];
    }
    // save original stdio
    og_stdin = dup(STDIN_FILENO);
    og_stdout = dup(STDOUT_FILENO);

    // open the dfa input file
    fp = fopen(filename, "r");
    if( fp == NULL){
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        exit(1);
    }


    // read the dfa file
    fscanf(fp, "%d", &s); 
    fscanf(fp, "%d", &n);
    for( i = 0; i < n; i++){
        fscanf(fp, "%d %c", &state_num, &final_marker);
        states[state_num].is_final = ( final_marker == 'F') ? 1 : 0;
        for( j = 0; j < s; j++){
            fscanf(fp, "%d", &states[state_num].transition[j]);
        }
    }

    fclose(fp);

    for( int i =0;i<= n; i++){
        if(pipe(fd[i])==-1){
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
            state_loop(i);
            // child created and waits at read end for state information
            exit(0);  
        }
    }

    sleep(1); // parents waits for 1 sec for child to be ready at read
    
    // send the state info
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


/*
dup and dup2 functions

The dup() function creates a copy of a given file descriptor and 
returns the lowest-numbered unused file descriptor number available
in the calling process to refer to the same open file. 

    Syntax: int dup(int oldfd);
    oldfd: The original file descriptor to duplicate.
    Return Value: The new file descriptor on success, or -1 if an error occurs.

The dup2() function is similar to dup(), 
but it allows the programmer to specify the exact number of the new file descriptor.
If a file is already open with the specified new descriptor number, it is closed first, atomically, before being reused. 

    Syntax: int dup2(int oldfd, int newfd);
    oldfd: The original file descriptor to duplicate.
    newfd: The desired number for the new file descriptor.
    Return Value: newfd on success, or -1 if an error occurs.
    
*/

/*  A pipe is treated as a file by the system. You must have used
	fopen() to open a file. fopen() returns a "file pointer" which
	is used in fprintf(), fscanf(), fclose() etc. 
    For each process the system maintains a "file descriptor table" (FDT) containing an entry 
    for each file opened by that process. When a new file is opened,
	a new entry is created in the FDT, and the entry number is
	returned as an integer called "file descriptor". 

fd[0] is for reading
fd[1] is for writing

*/
