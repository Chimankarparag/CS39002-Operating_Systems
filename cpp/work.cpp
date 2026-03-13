// work.cpp
// Implements the worker thread function.  Each of the M_total threads
// runs worker_func, identified by its integer ID w ∈ [0, M_total).
//
// Threading model (one car at a time):
//   bop barrier ──► workers process their to-do list ──► eop barrier
//
// Within the to-do loop, a worker holds mtx while reading / writing the
// shared PSTAT / WSTAT / WINFO arrays, and it releases mtx only when
// sleeping on its condition variable (pthread_cond_wait atomically releases
// mtx and puts the thread to sleep, then reacquires mtx on wakeup).

void* worker_func(void* arg) {
    int w = *(int*)arg;

    while (true) {

        /* ── Wait for the manager to set up the next car ── */
        pthread_barrier_wait(&bop);

        /* ── If the manager signalled end-of-all-work, print Quit and exit ── */
        if (all_done) {
            // No mutex needed here: every other thread has also seen all_done
            // and will no longer touch shared arrays.
            print_col(w, "Quit");
            pthread_exit(nullptr);
        }

        /* ── Determine this worker's to-do list for the current car ── */
        // Workers whose ID ≥ cur_car->M have no assigned parts.
        // Within cur_car->M, a worker's todo list may still be empty.
        bool has_parts = (w < cur_car->M) && (!cur_car->todo[w].empty());

        if (!has_parts) {
            // Nothing to do for this car — immediately declare done.
            pthread_mutex_lock(&mtx);
            WSTAT[w] = WDONE;
            print_col(w, "All done");
            pthread_mutex_unlock(&mtx);

        } else {
            const vector<int>& mytodo = cur_car->todo[w];

            for (int p : mytodo) {
                pthread_mutex_lock(&mtx);

                // Record what this worker intends to work on next.
                WSTAT[w] = WWORKING;
                WINFO[w] = p;

                /* ── Block until every prerequisite of p is complete ── */
                if (!all_prereqs_done(p)) {
                    WSTAT[w] = WWAITING;
                    print_col(w, "Wait " + to_string(p));

                    // The while-loop guards against spurious wakeups:
                    // pthread_cond_wait may return even without a signal,
                    // so we always re-check the condition.
                    while (!all_prereqs_done(p))
                        pthread_cond_wait(&cnd[w], &mtx);

                    // We have been woken up by the worker that completed
                    // the last remaining prerequisite of p.
                    WSTAT[w] = WWORKING;
                }

                /* ── Execute part p ── */
                print_col(w, "Part " + to_string(p));
                PSTAT[p] = PART_DONE;

                /* ── Check whether completing p unblocks any successor ── */
                // For each part q that has p as a prerequisite:
                for (int q : cur_car->deps[p]) {
                    if (all_prereqs_done(q)) {
                        // All prerequisites of q are now satisfied.
                        int v = cur_car->worker_of[q];
                        // Wake v only if it is currently waiting for exactly q.
                        // (It might not have reached q yet, in which case it
                        //  will check prereqs itself and not block at all.)
                        if (WSTAT[v] == WWAITING && WINFO[v] == q) {
                            print_col(w, "Wake up");
                            pthread_cond_signal(&cnd[v]);
                        }
                    }
                }

                pthread_mutex_unlock(&mtx);
            } // end for each part

            // Finished every part in the to-do list.
            pthread_mutex_lock(&mtx);
            WSTAT[w] = WDONE;
            print_col(w, "All done");
            pthread_mutex_unlock(&mtx);
        }

        /* ── Signal the manager (and peers) that this car is complete ── */
        pthread_barrier_wait(&eop);

    } // end while(true)

    return nullptr; // unreachable
}
