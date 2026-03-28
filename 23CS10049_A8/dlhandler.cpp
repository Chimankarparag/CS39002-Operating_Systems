#include "global.h"

static bool detectDeadlock(vector<int> &finish_seq) {
    vector<int>  work   = AVAILABLE;
    vector<bool> finish(N, false);

    for (int i = 0; i < N; ++i)
        if (STATUS[i] == WorkerStatus::EXITED) finish[i] = true;

    bool found = true;
    while (found) {
        found = false;
        for (int i = 0; i < N; ++i) {
            if (finish[i]) continue;

            bool ok = true;
            for (int j = 0; j < M; ++j)
                if (work[j] < REQUEST[i][j]) { ok = false; break; }

            if (ok) {
                for (int j = 0; j < M; ++j)
                    work[j] += ALLOCATION[i][j];
                finish[i] = true;
                finish_seq.push_back(i);
                found = true;
            }
        }
    }

    for (int i = 0; i < N; ++i)
        if (!finish[i]) return true;
    return false;
}

void *dlhandler_thread(void *arg) {
    (void)arg;

    while (true) {
        sleep(1);

        pthread_mutex_lock(&RMTX);

        if (NACTIVE == 0) {
            pthread_mutex_lock(&SMTX);
            REQTYPE = ReqType::QUIT;
            pthread_cond_signal(&SCND);
            pthread_mutex_unlock(&SMTX);
            pthread_mutex_unlock(&RMTX);
            printf("                                                All workers left\n");
            break;
        }

        vector<int> finish_seq;
        printf("\n                                                                      Deadlock detection in progress\n");

        bool deadlock = detectDeadlock(finish_seq);

        printf("                                                 Finish sequence:");
        for (int id : finish_seq) printf(" %d", id);
        printf("\n");

        if (!deadlock) {
            printf("                                                                                No deadlock detected\n\n");
            pthread_mutex_unlock(&RMTX);
            continue;
        }

        while (deadlock) {
            printf("                                                                                   Deadlock detected\n");

            printf("                    Allocation status:");
            for (int i = 0; i < N; ++i) {
                if (STATUS[i] == WorkerStatus::EXITED) continue;
                int total_alloc = 0;
                for (int j = 0; j < M; ++j) total_alloc += ALLOCATION[i][j];
                printf(" %d:%d", i, total_alloc);
            }
            printf("\n");

            int victim    = -1;
            int max_alloc = -1;

            vector<int> q_snap = rqSnap();
            for (int tid : q_snap) {
                if (STATUS[tid] == WorkerStatus::EXITED) continue;
                int tot = 0;
                for (int j = 0; j < M; ++j) tot += ALLOCATION[tid][j];
                if (tot > max_alloc || (tot == max_alloc && tid < victim)) {
                    max_alloc = tot;
                    victim    = tid;
                }
            }

            if (victim == -1) break;

            printf("                                               Preempting resources from worker %2d with %2d resources\n",
                   victim, max_alloc);

            for (int j = 0; j < M; ++j) {
                AVAILABLE[j]          += ALLOCATION[victim][j];
                ALLOCATION[victim][j]  = 0;
            }

            resolveReq();

            finish_seq.clear();
            printf("\n                                                                      Deadlock detection in progress\n");
            deadlock = detectDeadlock(finish_seq);

            printf("                                                 Finish sequence:");
            for (int id : finish_seq) printf(" %d", id);
            printf("\n");
        }

        if (!deadlock)
            printf("                                                                                No deadlock detected\n\n");

        pthread_mutex_unlock(&RMTX);
    }

    return nullptr;
}
