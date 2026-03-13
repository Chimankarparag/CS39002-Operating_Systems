#include <iostream>
#include <vector>
#include <string>
#include <pthread.h>

// limits 
#define MAXN 100    // max number of parts
#define MAXM 100    // max number of workers
#define MAXDEP 100  // max edges per node 

// number of cars
int n_fc;   // number of foocar arg
int n_bc;   // number of barcar arg

// num parts for cars
int N[2];       // 0-> parts of foocar, 1-> parts of barcar
int Mcar[2];    // workers for foocar and barcar  
int M;          // max of both workers

// Car_type
enum car_type{
    FOOCAR,
    BARCAR
};

// worker state 
enum w_state{
    START,
    WORKING,
    WAITING,
    DONE
};

// part status 
enum p_status{
    PENDING,
    DONE
};