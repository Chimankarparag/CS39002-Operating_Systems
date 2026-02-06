#include<stdio.h>
#include<sys/wait.h>
#include<stdbool.h>
#include <stdlib.h>
#include <unistd.h> // for unix io

#define MAX_NODES 30
#define MAX_DEG 30

int n; // number of nodes
int adj[MAX_NODES + 1][MAX_DEG]; // adjacency list
int deg[MAX_NODES + 1]; // degree of each node

void load_gengraph(){

    FILE *fptr;

    fptr = fopen("graph.txt","r");
    if (fptr == NULL) {
    printf("Error! Could not open file\n");
    // Program exits if the file pointer returns NULL
    exit(1);
    }

    fscanf(fptr,"%d\n",&n);

    //for lines in graph.txt
    for(int i = 1 ;i<=n;i++){
        deg[i]=0;

        int u; 
        fscanf(fptr, "%d ->", &u); // this line is hardcoded by %d -> because thats how its given in generated graph
        int v;
        while(fscanf(fptr,"%d",&v)==1){
            //read untill get integer
            adj[u][deg[u]++]=v;
            if(fgetc(fptr)=='\n') break;
        }
    }
    fclose(fptr);
}

int visited(int v, int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (atoi(argv[i]) == v)
            return 1;
    }
    return 0;
}

int main(int argc, char *argv[] ){
    load_gengraph();

    //since the child also call the same program 
    /*
        ./runfile { partial path till the parent }
        here for each consequent child the partial path increase
        decrease if backtrack
        So we measure the current path using the ArgumentC passed in child processes

    */

    int current = argc -1;

    // New Process starts here 
    printf("*** Process %d:", getpid());
    for (int i = 1; i < argc; i++)
        printf(" %s", argv[i]);
    printf("\n");



    // ROOT Process
    if(current == 0 ){
    
        // base case, we create a child process and pass the first node in the cycle
        // Here Assuming first node to be "1"

        pid_t cpid = fork();

        /* CHILD */
        if(cpid == 0){
            char *args[] = {"./a.out", "1", NULL};
            execv(args[0], args);
            exit(1);
        }

        /* ROOT PARENT */
        int status;
        waitpid(cpid,&status,0); //pid of child, status, 0== wait till exit called
        
        //extract the exit() return from status
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            exit(0);
        else {
            printf("\nNo Hamiltonian cycle found\n");
            exit(1);
        }
    }

    int last_node = atoi(argv[current]);
    int start_node = atoi(argv[1]);


    // LEAF NODE PROCESS (current == n)

    if(current == n){
        for( int i = 0 ;i<deg[last_node];i++){
            if(adj[last_node][i] == start_node ){
                printf("\nHamiltonian Cycle Found: ");
                for (int j = 1; j < argc; j++)
                    printf("%s ", argv[j]);
                printf("1\n");
                exit(0);
            }
        }
        exit(1);

    }

    // INTERMEDIATE NODES PROCESS

    /*  
     for new unvisited neighbour
        make a child
        copy the old partial list in new bigger list, 
        add the new unvisited neighbour node to path
        call the child process with updated arguement list 
        wait for child at the end 
        pass the exit status to the parent when child is terminated

    */

    for( int i = 0 ;i<deg[last_node];i++){
        int neighbour = adj[last_node][i];
        if (visited(neighbour, argc, argv))
            continue;

        pid_t cpid = fork();
        /* Intermediate CHILD*/
        if(cpid == 0){
            char *update_argv[MAX_NODES + 2];
            for(int k = 0 ;k < argc ;k++ ){
                update_argv[k] = argv[k]; //copy previous partial path
            }
            char *buf = malloc(8);
            sprintf(buf, "%d", neighbour);
            update_argv[argc] = buf;
            update_argv[argc + 1] = NULL;

            execv(argv[0], update_argv);
            exit(1);
        }

        int status;
        waitpid(cpid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            exit(0);
    }

    exit(1);

}