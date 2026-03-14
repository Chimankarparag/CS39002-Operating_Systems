// global.cpp
// Defines all shared data structures, global state, synchronisation
// primitives, and small utility helpers used by every other file.

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

/* ── Part-status codes ──────────────────────────────────────────────── */
#define PENDING   0
#define PART_DONE 1

/* ── Worker-status codes ────────────────────────────────────────────── */
#define WSTART    0   // thread created, not yet at bop
#define WWORKING  1   // currently working on a part
#define WWAITING  2   // blocked, waiting for a prerequisite
#define WDONE     3   // finished every part in to-do list for this car

/* ── Per-car specification ──────────────────────────────────────────── */
class CarData {
    public:
    int N{0}, M{0};             // #parts, #workers dedicated to this car type
    string name;                // "Foocar" or "Barcar"

    vector<vector<int>> deps;    // deps[p]    = parts that have p as prerequisite
    vector<vector<int>> prereqs; // prereqs[p] = prerequisites of p
    vector<int>         worker_of; // which worker is responsible for part p
    vector<vector<int>> todo;    // sorted to-do list for each worker

    // Read the schedule file and build all tables.
    // File format per line: <part> <worker> <successor1> … <successorK> -1
    void read(const string& fname, const string& cname) {
        name = cname;
        ifstream fin(fname);
        if (!fin) {
            fprintf(stderr, "Cannot open schedule file: %s\n", fname.c_str());
            exit(1);
        }

        fin >> N >> M;
        deps.assign(N, {});
        prereqs.assign(N, {});
        worker_of.resize(N);
        todo.assign(M, {});

        for (int i = 0; i < N; i++) {
            int p, w;
            fin >> p >> w;
            worker_of[p] = w;
            todo[w].push_back(p);

            int x;
            while (fin >> x && x != -1) {
                // Edge p → x : "p must be done before x"
                deps[p].push_back(x);    // x is a successor of p
                prereqs[x].push_back(p); // p is a prerequisite of x
            }
        }

        // Each worker's list must be processed in ascending part-number order.
        for (auto& v : todo) sort(v.begin(), v.end());
    }

    // Pretty-print dependencies, prerequisites and worker assignments.
    void print_info() const {
        printf("+++ %s\n", name.c_str());

        printf("   Dependencies\n");
        for (int p = 0; p < N; p++) {
            printf("   %2d ->", p);
            for (int d : deps[p]) printf(" %d", d);
            printf("\n");
        }

        printf("   Prerequisites\n");
        for (int p = 0; p < N; p++) {
            printf("   %2d <-", p);
            for (int q : prereqs[p]) printf(" %d", q);
            printf("\n");
        }

        printf("   Worker assignment\n");
        for (int w = 0; w < M; w++) {
            printf("   %2d :", w);
            for (int p : todo[w]) printf(" %d", p);
            printf("\n");
        }
    }
};

/* ── Global car data ────────────────────────────────────────────────── */
CarData foocar, barcar;
int     M_total;           // max(foocar.M, barcar.M) — actual thread count

/* ── Production sequence and current-car pointer ────────────────────── */
vector<int> prod_seq;      // 0 = foocar, 1 = barcar
CarData*    cur_car = nullptr;
bool        all_done = false; // set by main when every car has been built

/* ── Per-production status arrays ───────────────────────────────────── */
// Sized to max(N_foo, N_bar) / M_total at startup; reset before each car.
vector<int> PSTAT; // PENDING or PART_DONE for each part
vector<int> WSTAT; // WSTART / WWORKING / WWAITING / WDONE for each worker
vector<int> WINFO; // part the worker is working on or waiting for

/* ── Synchronisation primitives ─────────────────────────────────────── */
pthread_barrier_t bop; // begin-of-production barrier  (M_total + 1 participants)
pthread_barrier_t eop; // end-of-production barrier    (M_total + 1 participants)
pthread_mutex_t   mtx; // single mutex for all shared arrays
pthread_cond_t*   cnd = nullptr; // cnd[w]: worker w waits here for its prerequisite

/* ── Utility helpers ────────────────────────────────────────────────── */

// Returns true if every prerequisite of part p (in cur_car) is DONE.
// MUST be called while holding mtx.
bool all_prereqs_done(int p) {
    for (int pre : cur_car->prereqs[p])
        if (PSTAT[pre] == PENDING) return false;
    return true;
}

// Print the "WORKER 0  WORKER 1  …" header row used at the start of each car.
// Each column occupies exactly 10 characters.
void print_header() {
    printf("  ");
    for (int w = 0; w < M_total; w++) printf("WORKER %-2d ", w);
    printf("\n  ");
    for (int w = 0; w < M_total; w++) printf("--------- ");
    printf("\n");
}

// Print a single action 'act' in worker w's column.
// Preceding columns are printed as blank space (10 chars each).
// Call while holding mtx (or when no concurrency is possible).
void print_col(int w, const string& act) {
    printf("  ");
    for (int i = 0; i < w; i++) printf("          "); // 10 spaces per column
    printf("%-9s\n", act.c_str());
}
