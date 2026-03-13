/* global.cpp – all defines, types, and global variable definitions */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

#ifdef __APPLE__
#include <errno.h>
/* pthread barrier shim for macOS */
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

/* ─── Limits ─────────────────────────────────────────── */
#define MAXN   500
#define MAXM   100
#define MAXDEP 500

/* ─── Part status ────────────────────────────────────── */
#define PENDING  0
#define DONE     1

/* ─── Worker status ──────────────────────────────────── */
#define WSTART   0
#define WWORKING 1
#define WWAITING 2
#define WDONE    3

/* ─── Car types ──────────────────────────────────────── */
#define FOOCAR 0
#define BARCAR 1

/* ─── Car specifications (read-only after init) ──────── */
int N[2];
int Mcar[2];
int M;

int ndeps[2][MAXN];
int deps[2][MAXN][MAXDEP];

int nprereqs[2][MAXN];
int prereqs[2][MAXN][MAXDEP];

int worker_of[2][MAXN];

int ntodo[2][MAXM];
int todo[2][MAXM][MAXN];

/* ─── Per-production state ───────────────────────────── */
int current_car;
int production_over;

int PSTAT[MAXN];
int WSTAT[MAXM];
int WINFO[MAXM];
int prereq_done[MAXN];

/* ─── Synchronization ───────────────────────────────── */
pthread_barrier_t bop;
pthread_barrier_t eop;
pthread_mutex_t   mtx;
pthread_cond_t    cnd[MAXM];

/* ─── Thread IDs ────────────────────────────────────── */
pthread_t tids[MAXM];

/* ─── Print one row of the production table ─────────────
   events[w] != nullptr → print that string in worker-w column.
   events[w] == nullptr → print blank column.
   Caller must hold mtx.
   ─────────────────────────────────────────────────────── */
void print_event_line(char **events)
{
    for (int w = 0; w < M; w++) {
        if (events[w])
            std::printf(" %-8s", events[w]);
        else
            std::printf("         ");
    }
    std::printf("\n");
    std::fflush(stdout);
}
