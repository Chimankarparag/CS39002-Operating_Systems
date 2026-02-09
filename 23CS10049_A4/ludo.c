// 23CS10049 Parag Mahadeo Chimankar

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>

#define BOARDSIZE 101
#define MAXPLAYERS 26
#define PIPE_READ 0
#define PIPE_WRITE 1

int read_board(int *board ){

    FILE *fp = fopen("ludo.txt","r");
    if(!fp){
        perror("Error: File opening\n");
        return -1;
    }
    //init board to zero
    for( int i =0;i<BOARDSIZE;i++){
        board[i]=0;
    }

    char type;
    int from, to;

    while(fscanf(fp," %c",&type)==1){
        if(type=='E'){
            break;
        }

        if(type=='L' || type=='S'){
            if(fscanf(fp,"%d %d",&from,&to)==2){
                board[from]=to-from;
            }
        }

    }
    fclose(0);
    return 0;

}

int main(int argc, char* argv[]){
    if(argc!=2){
        fprintf(stderr, "Usage: %s <num_players>\n", argv[0]);
        exit(1);
    }

    int num_players = atoi(argv[1]);
    if(num_players<1 || num_players >MAXPLAYERS){
        fprintf(stderr,"Number of players should be between 1 and %d\n",MAXPLAYERS);
        exit(1);
    }

    //printf("debug: init shared memory key\n");

    key_t key_MB = ftok(".",'B');
    key_t key_MP = ftok(".",'P');
    
    int shmid_MB = shmget(key_MB, BOARDSIZE*sizeof(int),IPC_CREAT|IPC_EXCL|0666);
    if(shmid_MB==-1){
        perror("shmid_MB");
        exit(1);
    }

    int shmid_MP = shmget(key_MP,(num_players+1)*sizeof(int),IPC_CREAT|IPC_EXCL|0666);
    if(shmid_MP==-1){
        perror("shmid_MP");
        shmctl(shmid_MB,IPC_RMID,NULL); //just in case the init fails, clean the shared memory of board
        exit(1);
    }

    int *MB = (int *)shmat(shmid_MB,NULL,0); // initially RnW, populate then READ Only
    int *MP = (int *)shmat(shmid_MP,NULL,0);

        if (MB == (int *)-1 || MP == (int *)-1) {
        perror("shmat");
        shmctl(shmid_MB, IPC_RMID, NULL);
        shmctl(shmid_MP, IPC_RMID, NULL);
        exit(1);
    }

    //printf("debug: init the MB\n");
    // init MB
    if (read_board(MB) == -1) {
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_MB, IPC_RMID, NULL);
        shmctl(shmid_MP, IPC_RMID, NULL);
        exit(1);
    }

    shmdt(MB); 
    struct shmid_ds shm_desc;
    shmctl(shmid_MB, IPC_STAT, &shm_desc);
    shm_desc.shm_perm.mode = 0444; 
    shmctl(shmid_MB, IPC_SET, &shm_desc);

    // for reattaching
    // MB = (int *)shmat(shmid_MB,SHM_RDONLY,0);

    //init MP
    for(int i = 0;i<num_players;i++){
        MP[i]=0;
    }
    MP[num_players]=num_players; //initially all active players

    // pipe
    int pfd[2];
    if(pipe(pfd)==-1){
        perror("pipe");
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_MB, IPC_RMID, NULL);
        shmctl(shmid_MP, IPC_RMID, NULL);
        exit(1);
    }
    
    // to pass as command line arguement we need to convert [int]->[string]
    char str_pfd_write[20], str_num_players[20];
    sprintf(str_pfd_write,"%d",pfd[PIPE_WRITE]);
    sprintf(str_num_players,"%d",num_players);

    pid_t XBP_pid = fork();
        if (XBP_pid == -1) {
        perror("fork XBP");
        close(pfd[PIPE_READ]);
        close(pfd[PIPE_WRITE]);
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_MB, IPC_RMID, NULL);
        shmctl(shmid_MP, IPC_RMID, NULL);
        exit(1);
    }

    if (XBP_pid == 0) {
        close(pfd[PIPE_READ]);  
        
        execlp("xterm", "xterm", 
               "-T", "Board",           // Title
               "-fs", "15",             // Font size
               "-geometry", "150x24+50+100",  // Columns x Rows + X + Y
               "-bg", "#ffffff",        // Background
               "-e", "./board", str_num_players, str_pfd_write,
               NULL);
        
        perror("execlp xterm for board");
        exit(1);
    }

    //printf("debug: XBP created\n");

    pid_t bp_pid;
    if (read(pfd[PIPE_READ], &bp_pid, sizeof(pid_t)) != sizeof(pid_t)) {
        perror("read BP PID");
        kill(XBP_pid, SIGKILL);
        waitpid(XBP_pid, NULL, 0);
        close(pfd[PIPE_READ]);
        close(pfd[PIPE_WRITE]);
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_MB, IPC_RMID, NULL);
        shmctl(shmid_MP, IPC_RMID, NULL);
        exit(1);
    }

    char str_bp_pid[20];
    sprintf(str_bp_pid, "%d", bp_pid);

    pid_t XPP_pid = fork();
    if (XPP_pid == -1) {
        perror("fork XPP");
        close(pfd[PIPE_READ]);
        close(pfd[PIPE_WRITE]);
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_MB, IPC_RMID, NULL);
        shmctl(shmid_MP, IPC_RMID, NULL);
        exit(1);
    }

    if (XPP_pid == 0) {
        close(pfd[PIPE_READ]);  
        
        execlp("xterm", "xterm",
               "-T", "Players",         // Title
               "-fs", "15",             // Font size
               "-geometry", "100x24+1000+100",  // Columns x Rows + X + Y
               "-bg", "#ffffff",        // Background
               "-e", "./players", str_num_players, str_pfd_write, str_bp_pid,
               NULL);

        perror("execlp xterm for players");
        exit(1);
    }

    //printf("debug: XPP created\n");

    pid_t pp_pid;
    if (read(pfd[PIPE_READ], &pp_pid, sizeof(pid_t)) != sizeof(pid_t)) {
        perror("read PP PID");
        kill(XPP_pid, SIGKILL);
        kill(XBP_pid, SIGKILL);
        waitpid(XPP_pid, NULL, 0);
        waitpid(XBP_pid, NULL, 0);
        close(pfd[PIPE_READ]);
        close(pfd[PIPE_WRITE]);
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_MB, IPC_RMID, NULL);
        shmctl(shmid_MP, IPC_RMID, NULL);
        exit(1);
    }

    char ack;
    if (read(pfd[PIPE_READ], &ack, 1) != 1) {
        perror("read initial ack from BP");
        kill(XPP_pid, SIGKILL);
        kill(XBP_pid, SIGKILL);
        waitpid(XPP_pid, NULL, 0);
        waitpid(XBP_pid, NULL, 0);
        close(pfd[PIPE_READ]);
        close(pfd[PIPE_WRITE]);
        shmdt(MB);
        shmdt(MP);
        shmctl(shmid_MB, IPC_RMID, NULL);
        shmctl(shmid_MP, IPC_RMID, NULL);
        exit(1);
    }

    close(pfd[PIPE_WRITE]);

    //printf("debug: bp, pp and ack initialised\n");

    int autoplay = 0;
    int delay_ms = 1000;    // Default 
    char command[100];

    while (1) {
        if (autoplay) {

            usleep(delay_ms * 1000); 
        
            if (MP[num_players] == 0) {
                printf("\nAll players have reached the destination!\n");
                printf("Hit return to end the game...\n");
                fflush(stdout);
                getchar();
                break;
            }
            kill(pp_pid, SIGUSR1);
            
            if (read(pfd[PIPE_READ], &ack, 1) != 1) {
                break; 
            }

        } else {
            // Interactive mode: wait for user command
            printf("Enter command: ");
            fflush(stdout);
            
            if (fgets(command, sizeof(command), stdin) == NULL) {
                break;
            }
            
            command[strcspn(command, "\n")] = 0;
            
            if (strcmp(command, "next") == 0) {
                if (MP[num_players] == 0) {
                    printf("Game is already over!\n");
                    printf("Hit return to end the game...\n");
                    fflush(stdout);
                    getchar();
                    break;
                }
            
                kill(pp_pid, SIGUSR1);
            
                if (read(pfd[PIPE_READ], &ack, 1) != 1) {
                    break; 
                }

            } else if (strncmp(command, "autoplay", 8) == 0) {
                int custom_delay;
                if (sscanf(command, "autoplay %d", &custom_delay) == 1) {
                    delay_ms = custom_delay;
                }
                printf("Starting autoplay...\n");
                printf("Delay(ms) = %d\n",delay_ms);
                autoplay = 1;
            } else if (strcmp(command, "quit") == 0) {
                printf("Quitting game...\n");
                printf("Hit return to end the game...\n");
                fflush(stdout);
                getchar();
                break;
            } else {
                printf("Unknown command: %s\n", command);
                printf("Valid commands: next, autoplay [delay_ms], quit\n");
            }
        }
    }
    

    printf("\nTerminating...\n");
    kill(pp_pid, SIGUSR2);
    waitpid(XPP_pid, NULL, 0);
    printf("Player processes terminated.\n");

    kill(bp_pid, SIGUSR2);
    waitpid(XBP_pid, NULL, 0);
    printf("Board process terminated.\n");

    close(pfd[PIPE_READ]);

    shmdt(MB);
    shmdt(MP);
    shmctl(shmid_MB, IPC_RMID, NULL);
    shmctl(shmid_MP, IPC_RMID, NULL);
    printf("Game ended.\n");
    
    return 0;
}
