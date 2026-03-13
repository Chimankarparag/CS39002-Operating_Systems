// manager.cpp
// Implements run_manager(), which is called by main().
//
// Responsibilities:
//  1. Build the alternating foocar/barcar production sequence.
//  2. Initialise all pthreads synchronisation resources.
//  3. Create the M_total worker threads.
//  4. Drive the per-car bop → (workers work) → eop cycle.
//  5. Signal workers to quit and join them cleanly.

// Forward declaration: worker_func is defined in work.cpp, which is
// included after this file inside foobar.cpp.
void* worker_func(void* arg);

// Build the production order for f foocars and b barcars.
// The cars alternate (foo, bar, foo, bar, …) for min(f,b) pairs,
// then the remaining cars of the more-numerous type are appended.
// Example: f=4, b=2  →  [foo, bar, foo, bar, foo, foo]
static void build_prod_seq(int f, int b) {
    int pairs = min(f, b);
    for (int i = 0; i < pairs; i++) {
        prod_seq.push_back(0); // foocar
        prod_seq.push_back(1); // barcar
    }
    // Append leftover foocars or barcars.
    if (f > b)
        for (int i = b; i < f; i++) prod_seq.push_back(0);
    else
        for (int i = f; i < b; i++) prod_seq.push_back(1);
}

void run_manager(int f, int b) {

    /* ── Build the production sequence ─────────────────────────────── */
    build_prod_seq(f, b);
    M_total = max(foocar.M, barcar.M);

    /* ── Initialise synchronisation objects ────────────────────────── */
    // Both barriers need M_total workers + 1 manager thread to complete.
    pthread_barrier_init(&bop, nullptr, M_total + 1);
    pthread_barrier_init(&eop, nullptr, M_total + 1);
    pthread_mutex_init(&mtx, nullptr);

    // One condition variable per worker: cnd[w] is the waiting place for
    // worker w when its next part has unsatisfied prerequisites.
    cnd = new pthread_cond_t[M_total];
    for (int w = 0; w < M_total; w++)
        pthread_cond_init(&cnd[w], nullptr);

    /* ── Allocate shared status arrays ─────────────────────────────── */
    // Size them to the larger of the two car types so we never reallocate.
    int max_N = max(foocar.N, barcar.N);
    PSTAT.assign(max_N,    PENDING);
    WSTAT.assign(M_total,  WSTART);
    WINFO.assign(M_total,  0);

    /* ── Create worker threads ──────────────────────────────────────── */
    // IDs are kept in a vector so their addresses remain stable for the
    // lifetime of the threads (argv trick: pass &ids[w], not a local int).
    vector<pthread_t> threads(M_total);
    vector<int>       ids(M_total);
    for (int w = 0; w < M_total; w++) {
        ids[w] = w;
        pthread_create(&threads[w], nullptr, worker_func, &ids[w]);
    }

    /* ── Main production loop ───────────────────────────────────────── */
    for (int car_type : prod_seq) {

        cur_car = (car_type == 0) ? &foocar : &barcar;
        const char* cname = (car_type == 0) ? "foocar" : "barcar";

        printf("+++ Production of a %s begins\n", cname);
        print_header();

        // Reset per-part and per-worker status for this car.
        // Workers will read these AFTER passing the bop barrier, so
        // writing them here (before bop) is safe — the barrier itself
        // acts as the necessary memory fence.
        fill(PSTAT.begin(), PSTAT.begin() + cur_car->N, PENDING);
        fill(WSTAT.begin(), WSTAT.end(),                  WSTART);

        // Release all workers to begin production.
        pthread_barrier_wait(&bop);

        // Block until all workers have finished this car.
        pthread_barrier_wait(&eop);
    }

    /* ── Signal end of all productions ────────────────────────────── */
    // Setting all_done = true BEFORE the bop barrier guarantees that
    // every worker reads the updated value after the barrier's memory fence.
    all_done = true;
    printf("+++ All productions completed\n");
    print_header();

    // This final bop wakes every worker so each one checks all_done,
    // prints "Quit", and calls pthread_exit.  There is no matching eop
    // because the workers exit immediately after this barrier.
    pthread_barrier_wait(&bop);

    /* ── Wait for all workers to terminate ─────────────────────────── */
    for (int w = 0; w < M_total; w++)
        pthread_join(threads[w], nullptr);

    /* ── Release all synchronisation resources ──────────────────────── */
    pthread_barrier_destroy(&bop);
    pthread_barrier_destroy(&eop);
    pthread_mutex_destroy(&mtx);
    for (int w = 0; w < M_total; w++)
        pthread_cond_destroy(&cnd[w]);
    delete[] cnd;
    cnd = nullptr;
}
