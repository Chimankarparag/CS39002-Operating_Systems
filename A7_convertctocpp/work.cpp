/* work.cpp – worker thread implementation */
#include <cstdio>
#include <cstring>

/* ────────────────────────────────────────────────────────────
 * worker  –  main function for each worker thread
 * ──────────────────────────────────────────────────────────── */
void *worker(void *arg)
{
    int w = static_cast<int>(reinterpret_cast<long>(arg));

    char  bufs[MAXM][16];
    char *events[MAXM];

    while (true) {

        /* ── Wait for manager to start the next production ─── */
        pthread_barrier_wait(&bop);

        /* ── Quit? ──────────────────────────────────────────── */
        if (production_over) {
            pthread_mutex_lock(&mtx);
            std::memset(events, 0, sizeof(events));
            std::sprintf(bufs[w], "Quit");
            events[w] = bufs[w];
            print_event_line(events);
            pthread_mutex_unlock(&mtx);
            pthread_exit(nullptr);
        }

        int t    = current_car;
        int my_n = ntodo[t][w];

        /* ── No parts assigned ──────────────────────────────── */
        if (my_n == 0) {
            pthread_mutex_lock(&mtx);
            WSTAT[w] = WDONE;
            std::memset(events, 0, sizeof(events));
            std::sprintf(bufs[w], "All done");
            events[w] = bufs[w];
            print_event_line(events);
            pthread_mutex_unlock(&mtx);

        } else {

            for (int i = 0; i < my_n; i++) {
                int p = todo[t][w][i];

                pthread_mutex_lock(&mtx);

                /* ── WAIT phase ──────────────────────────────── */
                if (prereq_done[p] < nprereqs[t][p]) {
                    WSTAT[w] = WWAITING;
                    WINFO[w] = p;

                    std::memset(events, 0, sizeof(events));
                    std::sprintf(bufs[w], "Wait %d", p);
                    events[w] = bufs[w];
                    print_event_line(events);

                    while (prereq_done[p] < nprereqs[t][p])
                        pthread_cond_wait(&cnd[w], &mtx);
                }

                /* ── WORK phase ──────────────────────────────── */
                WSTAT[w] = WWORKING;
                WINFO[w] = p;
                PSTAT[p] = DONE;

                std::memset(events, 0, sizeof(events));
                std::sprintf(bufs[w], "Part %d", p);
                events[w] = bufs[w];

                for (int j = 0; j < ndeps[t][p]; j++) {
                    int q = deps[t][p][j];
                    prereq_done[q]++;

                    if (prereq_done[q] == nprereqs[t][q]) {
                        int v = worker_of[t][q];
                        if (WSTAT[v] == WWAITING && WINFO[v] == q) {
                            std::sprintf(bufs[v], "Wake up");
                            events[v] = bufs[v];
                            pthread_cond_signal(&cnd[v]);
                        }
                    }
                }

                print_event_line(events);
                pthread_mutex_unlock(&mtx);
            }

            /* ── All parts done ──────────────────────────────── */
            pthread_mutex_lock(&mtx);
            WSTAT[w] = WDONE;
            std::memset(events, 0, sizeof(events));
            std::sprintf(bufs[w], "All done");
            events[w] = bufs[w];
            print_event_line(events);
            pthread_mutex_unlock(&mtx);
        }

        pthread_barrier_wait(&eop);
    }

    return nullptr;
}
