#ifndef GLOBAL_H
#define GLOBAL_H

#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <vector>
#include <queue>
#include <cstring>
using namespace std;

enum class WorkerStatus {
    ACTIVE,
    EXITED
};

enum class ReqType {
    NONE,
    ALLOCATE,
    RELEASE,
    QUIT
};

extern int M;   // number of resource types
extern int N;   // number of worker threads
extern int R;   // allocation/release requests per worker

extern vector<int> TOTAL;       // TOTAL[j]         – total instances of resource j
extern vector<int> AVAILABLE;   // AVAILABLE[j]     – currently available
extern vector<vector<int>> ALLOCATION;  // ALLOCATION[i][j] – held by worker i
extern vector<vector<int>> REQUEST;     // REQUEST[i][j]    – pending alloc request of worker i
extern vector<vector<int>> RELMAT;      // RELMAT[i][j]     – pending release of worker i

extern int REQFROM;   // serial number of requesting worker (-1 = unset)
extern ReqType REQTYPE;   // ALLOCATE / RELEASE / QUIT / NONE

extern int NACTIVE;   // workers still running
extern vector<WorkerStatus> STATUS;    // STATUS[i]
extern vector<pthread_t> workerTID; // TID array
extern vector<bool> GRANTED;   // GRANTED flag for each worker

extern queue<int> RQ;

void rqRem(int id);
vector<int> rqSnap();

extern pthread_mutex_t RMTX; 

extern pthread_cond_t SCND; 
extern pthread_mutex_t SMTX;

extern pthread_cond_t ACND; 
extern pthread_mutex_t AMTX;

extern vector<pthread_cond_t> WCND;
extern vector<pthread_mutex_t> WMTX; 

void start();
void cleanup();

bool generate_request(int who, vector<int> &req);
bool generate_release(int who, vector<int> &rel);

void resolveReq();

inline void print_vector(const vector<int> &v) {
    printf(" [ ");
    for (int j = 0; j < (int)v.size(); ++j) {
        printf("%2d", v[j]);
        if (j < (int)v.size() - 1) printf(", ");
    }
    printf(" ]");
}

inline void print_available() {
    printf("                        AVAILABLE =");
    print_vector(AVAILABLE);
    printf("\n");
}

inline void print_waiting_queue() {
    printf("        Workers waiting: (");
    vector<int> snap = rqSnap();
    for (int i = 0; i < (int)snap.size(); ++i) {
        printf(" %2d", snap[i]);
        if (i < (int)snap.size() - 1) printf(",");
    }
    printf(" )\n");
}

#endif // GLOBAL_H
