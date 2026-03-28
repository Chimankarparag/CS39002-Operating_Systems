#include "global.h"

void resolveReq() {
    vector<int> snap = rqSnap();

    for (int tid : snap) {
        if (STATUS[tid] == WorkerStatus::EXITED) { rqRem(tid); continue; }

        bool ok = true;
        for (int j = 0; j < M; ++j)
            if (AVAILABLE[j] < REQUEST[tid][j]) { ok = false; break; }
        if (!ok) continue;

        for (int j = 0; j < M; ++j) {
            AVAILABLE[j] -= REQUEST[tid][j];
            ALLOCATION[tid][j] += REQUEST[tid][j];
        }
        printf("Worker %2d granted pending request  ", tid);
        print_vector(REQUEST[tid]);
        printf("\n");
        fill(REQUEST[tid].begin(), REQUEST[tid].end(), 0);
        rqRem(tid);
        print_available();

        pthread_mutex_lock(&WMTX[tid]);
        GRANTED[tid] = true;
        pthread_cond_signal(&WCND[tid]);
        pthread_mutex_unlock(&WMTX[tid]);
    }
}

void *worker_thread(void *arg) {
    int me = *static_cast<int *>(arg);
    delete static_cast<int *>(arg);

    vector<int> vec(M);
    bool first_request = true;

    for (int req_num = 0; req_num < R; ) {

        struct timespec ts = { 0, (200 + rand() % 301) * 1000000L };
        nanosleep(&ts, NULL);

        bool do_alloc;
        if (first_request) {
            do_alloc = true;
        } else {
            do_alloc = (rand() % 3 == 0); 
        }

        pthread_mutex_lock(&RMTX);

        bool nonzero = do_alloc ? generate_request(me, vec) : generate_release(me, vec);
        if (!nonzero) {
            pthread_mutex_unlock(&RMTX);
            continue; 
        }

        ++req_num;
        first_request = false; 

        REQFROM = me;
        REQTYPE = do_alloc ? ReqType::ALLOCATE : ReqType::RELEASE;
        GRANTED[me] = false;

        if (do_alloc) {
            REQUEST[me] = vec;
            printf("Worker %2d makes allocation request ", me);
        } else {
            RELMAT[me] = vec;
            printf("Worker %2d makes release request    ", me);
        }
        print_vector(vec);
        printf("\n");

        pthread_mutex_lock(&SMTX);
        pthread_cond_signal(&SCND);
        pthread_mutex_unlock(&SMTX);

        pthread_mutex_lock(&AMTX);
        pthread_cond_wait(&ACND, &AMTX);
        pthread_mutex_unlock(&AMTX);

        pthread_mutex_unlock(&RMTX);

        if (do_alloc) {
            pthread_mutex_lock(&WMTX[me]);
            while (!GRANTED[me])
                pthread_cond_wait(&WCND[me], &WMTX[me]);
            pthread_mutex_unlock(&WMTX[me]);
        }
    }

    pthread_mutex_lock(&RMTX);

    REQFROM   = me;
    REQTYPE   = ReqType::RELEASE;
    RELMAT[me] = ALLOCATION[me];

    printf("                                 Worker %2d going to quit\n", me);
    printf("                              Releasing allocation");
    print_vector(ALLOCATION[me]);
    printf("\n");

    pthread_mutex_lock(&SMTX);
    pthread_cond_signal(&SCND);
    pthread_mutex_unlock(&SMTX);

    pthread_mutex_lock(&AMTX);
    pthread_cond_wait(&ACND, &AMTX);
    pthread_mutex_unlock(&AMTX);

    pthread_mutex_unlock(&RMTX);

    pthread_mutex_lock(&RMTX);
    STATUS[me] = WorkerStatus::EXITED;
    --NACTIVE;
    pthread_mutex_unlock(&RMTX);

    return nullptr;
}
