#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "manager.c"
#include "work.c"
#include "global.c"


int main(int argc, char *argv[]){
    if(argc != 5){
        printf("Usage: ./foobar <num-foocar> <num-barcar> fooschedule.txt barschedule.txt");
        exit(1);
    }
    n_fc = atoi(argv[1]);
    n_bc = atoi(argv[2]);

    // read the text files to make the dependency and prerequisite graphs
    FILE *ffp;
    FILE *bfp;
    

    return 0;
}