/* manager.cpp – main-thread logic */
#include <cstdio>
#include <cstdlib>

void *worker(void *arg);  /* defined in work.cpp */

/* ────────────────────────────────────────────────────────────
 * read_car_file
 *   File format:
 *       N  M
 *       p  w  d1  d2 … -1
 *   p = part, w = assigned worker, di = parts that depend on p.
 * ──────────────────────────────────────────────────────────── */
static void read_car_file(int type, const char *filename)
{
    FILE *fp = std::fopen(filename, "r");
    if (!fp) {
        std::fprintf(stderr, "Error: cannot open '%s'\n", filename);
        std::exit(EXIT_FAILURE);
    }

    std::fscanf(fp, "%d %d", &N[type], &Mcar[type]);

    for (int p = 0; p < N[type]; p++) {
        ndeps[type][p]    = 0;
        nprereqs[type][p] = 0;
    }
    for (int w = 0; w < Mcar[type]; w++)
        ntodo[type][w] = 0;

    for (int i = 0; i < N[type]; i++) {
        int p, w;
        std::fscanf(fp, "%d %d", &p, &w);

        worker_of[type][p] = w;
        todo[type][w][ntodo[type][w]++] = p;

        int d;
        while (std::fscanf(fp, "%d", &d) == 1 && d != -1) {
            deps[type][p][ndeps[type][p]++]       = d;
            prereqs[type][d][nprereqs[type][d]++] = p;
        }
    }

    std::fclose(fp);
}

/* ────────────────────────────────────────────────────────────
 * print_car_info
 * ──────────────────────────────────────────────────────────── */
static void print_car_info(int type)
{
    std::printf("+++ %s\n", type == FOOCAR ? "Foocar" : "Barcar");

    std::printf("   Dependencies\n");
    for (int p = 0; p < N[type]; p++) {
        std::printf("   %2d ->", p);
        for (int j = 0; j < ndeps[type][p]; j++)
            std::printf(" %d", deps[type][p][j]);
        std::printf("\n");
    }

    std::printf("   Prerequisites\n");
    for (int p = 0; p < N[type]; p++) {
        std::printf("   %2d <-", p);
        for (int j = 0; j < nprereqs[type][p]; j++)
            std::printf(" %d", prereqs[type][p][j]);
        std::printf("\n");
    }

    std::printf("   Worker assignment\n");
    for (int w = 0; w < Mcar[type]; w++) {
        std::printf("   %2d :", w);
        for (int j = 0; j < ntodo[type][w]; j++)
            std::printf(" %d", todo[type][w][j]);
        std::printf("\n");
    }
    std::printf("\n");
}

/* ────────────────────────────────────────────────────────────
 * print_production_header
 * ──────────────────────────────────────────────────────────── */
static void print_production_header(int type)
{
    std::printf("+++ Production of a %s begins\n",
                type == FOOCAR ? "foocar" : "barcar");
    for (int w = 0; w < M; w++)
        std::printf(" WORKER %d", w);
    std::printf("\n");
    for (int w = 0; w < M; w++)
        std::printf(" --------");
    std::printf("\n");
    std::fflush(stdout);
}

/* ────────────────────────────────────────────────────────────
 * manager  –  called by main(); drives the entire session
 * ──────────────────────────────────────────────────────────── */
void manager(int f, int b, const char *foofile, const char *barfile)
{
    /* 1. Parse specification files */
    read_car_file(FOOCAR, foofile);
    read_car_file(BARCAR, barfile);
    M = (Mcar[0] > Mcar[1]) ? Mcar[0] : Mcar[1];

    /* 2. Print specifications */
    print_car_info(FOOCAR);
    print_car_info(BARCAR);

    /* 3. Initialise synchronisation primitives */
    pthread_barrier_init(&bop, nullptr, static_cast<unsigned>(M + 1));
    pthread_barrier_init(&eop, nullptr, static_cast<unsigned>(M + 1));
    pthread_mutex_init(&mtx, nullptr);
    for (int w = 0; w < M; w++)
        pthread_cond_init(&cnd[w], nullptr);

    production_over = 0;

    /* 4. Launch worker threads (JOINABLE) */
    for (int w = 0; w < M; w++) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        pthread_create(&tids[w], &attr, worker, reinterpret_cast<void *>(static_cast<long>(w)));
        pthread_attr_destroy(&attr);
    }

    /* 5. Build alternating production sequence
          e.g. f=4, b=2  →  F B F B F F                              */
    int  total = f + b;
    int *seq   = new int[total];
    int  si    = 0, tf = f, tb = b;
    while (tf > 0 && tb > 0) { seq[si++] = FOOCAR; seq[si++] = BARCAR; tf--; tb--; }
    while (tf-- > 0) seq[si++] = FOOCAR;
    while (tb-- > 0) seq[si++] = BARCAR;

    /* 6. Production loop */
    for (int i = 0; i < total; i++) {
        int t = seq[i];
        current_car = t;

        for (int p = 0; p < N[t]; p++) {
            PSTAT[p]       = PENDING;
            prereq_done[p] = 0;
        }
        for (int w = 0; w < M; w++)
            WSTAT[w] = WSTART;

        print_production_header(t);

        pthread_barrier_wait(&bop);
        /* ── workers run the production here ── */
        pthread_barrier_wait(&eop);
    }

    delete[] seq;

    /* 7. Signal workers to quit */
    production_over = 1;

    std::printf("+++ All productions completed\n");
    for (int w = 0; w < M; w++) std::printf(" WORKER %d", w);
    std::printf("\n");
    for (int w = 0; w < M; w++) std::printf(" --------");
    std::printf("\n");
    std::fflush(stdout);

    pthread_barrier_wait(&bop);   /* workers see production_over==1, exit */

    /* 8. Join all worker threads */
    for (int w = 0; w < M; w++)
        pthread_join(tids[w], nullptr);

    /* 9. Clean up */
    pthread_barrier_destroy(&bop);
    pthread_barrier_destroy(&eop);
    pthread_mutex_destroy(&mtx);
    for (int w = 0; w < M; w++)
        pthread_cond_destroy(&cnd[w]);
}
