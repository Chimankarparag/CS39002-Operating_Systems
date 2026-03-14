#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>

using namespace std;

#ifdef __APPLE__
#include <errno.h>
#define PTHREAD_BARRIER_SERIAL_THREAD 1
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    unsigned count;
    unsigned waiting;
    unsigned phase;
} pthread_barrier_t;

static inline int pthread_barrier_init(pthread_barrier_t *barrier, const void *, unsigned count)
{
    if (count == 0) return EINVAL;
    pthread_mutex_init(&barrier->mutex, nullptr);
    pthread_cond_init(&barrier->cond, nullptr);
    barrier->count = count;
    barrier->waiting = 0;
    barrier->phase = 0;
    return 0;
}

static inline int pthread_barrier_wait(pthread_barrier_t *barrier)
{
    pthread_mutex_lock(&barrier->mutex);
    unsigned phase = barrier->phase;
    barrier->waiting++;
    if (barrier->waiting == barrier->count) {
        barrier->phase++;
        barrier->waiting = 0;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }
    while (phase == barrier->phase) {
        pthread_cond_wait(&barrier->cond, &barrier->mutex);
    }
    pthread_mutex_unlock(&barrier->mutex);
    return 0;
}

static inline int pthread_barrier_destroy(pthread_barrier_t *barrier)
{
    pthread_mutex_destroy(&barrier->mutex);
    pthread_cond_destroy(&barrier->cond);
    return 0;
}
#endif

enum class CarType { FOOCAR, BARCAR };
enum class WState { START, WORKING, WAITING, DONE };
enum class PStatus { PENDING, DONE };

class CarData{
    public:
        int N{0}, M{0}; // number of parts and workers for that 
        CarType type;
        string name;

        // dependency graph for that car parts (deps[N][0-MAXN])
        vector<vector<int>> deps; 
        // prereqiusites graph for part p
        vector<vector<int>> prereqs;
        // work assignment for worker w
        vector<vector<int>> workerAsgn;
        // worker responsible for the part
        vector<int> whichWorker;

        void read(const string& fname, CarType ctype, const string& cname){
            type = ctype;
            name = cname;

            ifstream fin(fname);
            if(!fin){
                fprintf(stderr, "Cannot open schedule file: %s\n", fname.c_str());
                exit(1);
            }

            fin >> N >> M;
            deps.assign(N,{});
            prereqs.assign(N,{});
            workerAsgn.assign(M,{});
            whichWorker.resize(N);

            for (int i = 0; i < N; i++)
            {
                int p, w;
                fin >> p >> w;
                whichWorker[p] = w;
                workerAsgn[w].push_back(p);

                int x;
                while( fin >> x && x!= -1){
                    // Edge p → x : "p must be done before x"
                    deps[p].push_back(x);       // x is a successor of p
                    prereqs[x].push_back(p);    // p is a prerequisite of x
                }
            }

            // Each worker's list must be processed in ascending part-number order.
            for (auto& v : workerAsgn) sort(v.begin(), v.end());

        }

        // Pretty-print dependencies, prerequisites and worker assignments.
        void print_info() const {
        printf("\n+++ %s\n", name.c_str());

        printf("\n   Dependencies\n");
        for (int p = 0; p < N; p++) {
            printf("   %2d ->", p);
            for (int d : deps[p]) printf(" %d", d);
            printf("\n");
        }

        printf("\n   Prerequisites\n");
        for (int p = 0; p < N; p++) {
            printf("   %2d <-", p);
            for (int q : prereqs[p]) printf(" %d", q);
            printf("\n");
        }

        printf("\n   Worker assignment\n");
        for (int w = 0; w < M; w++) {
            printf("   %2d :", w);
            for (int p : workerAsgn[w]) printf(" %d", p);
            printf("\n");
        }
    }
};

CarData Foocar, Barcar;
int M_total;    // max(foocar.M, barcar.M)

vector<CarType> prodSeq;
CarData *currCar = nullptr;
bool allDone = false;

vector <PStatus> PSTAT; // pending or done for each part
vector <WState> WSTAT; // start, working, waiting, done
vector <int> WINFO; // assigned part number, for which worker is waiting for

pthread_mutex_t mtx;
vector<pthread_cond_t> cnd; // cnd[w] is the waiting place for the worker w

pthread_barrier_t bop;  // begin of production, used by all threads
pthread_barrier_t eop;  // end of production, used by all threads

// utiily functions 

void printWorker() {
    printf("\n  ");
    for (int w = 0; w < M_total; w++) printf("WORKER %-2d ", w);
    printf("\n  ");
    for (int w = 0; w < M_total; w++) printf("--------- ");
    printf("\n");
}

void printCol(int w, const string& act) {
    printf("  ");
    for (int i = 0; i < w; i++) printf("          "); // 10 spaces per column
    printf("%-9s\n", act.c_str());
}

bool prereqsDone(int p) {
    for (int pre : currCar->prereqs[p])
        if (PSTAT[pre] == PStatus::PENDING) return false;
    return true;
}




