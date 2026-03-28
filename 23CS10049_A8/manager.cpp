#include "global.h"
#include "worker.cpp"   
#include "dlhandler.cpp" 

static void managerLoop() {
    while (true) {
        pthread_mutex_lock(&SMTX);
        pthread_cond_wait(&SCND, &SMTX);
        pthread_mutex_unlock(&SMTX);

        if (REQTYPE == ReqType::QUIT) return;

        int from = REQFROM;
        ReqType type = REQTYPE;

        if (type == ReqType::RELEASE) {
            for (int j = 0; j < M; ++j) {
                AVAILABLE[j] += RELMAT[from][j];
                ALLOCATION[from][j] -= RELMAT[from][j];
                RELMAT[from][j] = 0;
            }
            print_available();
            if (!RQ.empty()) {
                print_waiting_queue();
                resolveReq();
            }

            pthread_mutex_lock(&AMTX);
            pthread_cond_signal(&ACND);
            pthread_mutex_unlock(&AMTX);

        } else {
            bool can = true;
            for (int j = 0; j < M; ++j)
                if (REQUEST[from][j] > AVAILABLE[j]) { can = false; break; }

            if (can) {
                for (int j = 0; j < M; ++j) {
                    AVAILABLE[j] -= REQUEST[from][j];
                    ALLOCATION[from][j] += REQUEST[from][j];
                    REQUEST[from][j] = 0;
                }
                printf("Worker %2d granted request\n", from);
                print_available();

                pthread_mutex_lock(&WMTX[from]);
                GRANTED[from] = true;
                pthread_cond_signal(&WCND[from]);
                pthread_mutex_unlock(&WMTX[from]);

                pthread_mutex_lock(&AMTX);
                pthread_cond_signal(&ACND);
                pthread_mutex_unlock(&AMTX);

            } else {
                printf("Worker %2d has to wait\n", from);
                RQ.push(from);
                print_waiting_queue();

                pthread_mutex_lock(&AMTX);
                pthread_cond_signal(&ACND);
                pthread_mutex_unlock(&AMTX);
            }
        }
    }
}

int main(int argc, char *argv[]) {

    M = (argc > 1) ? atoi(argv[1]) : 8;
    N = (argc > 2) ? atoi(argv[2]) : 16;
    R = (argc > 3) ? atoi(argv[3]) : 32;

    setbuf(stdout, NULL);  
    srand((unsigned)time(NULL));

    start();

    printf("                            TOTAL =");
    print_vector(TOTAL);
    printf("\n");

    pthread_t dlh_tid;
    pthread_create(&dlh_tid, nullptr, dlhandler_thread, nullptr);

    for (int i = 0; i < N; ++i) {
        int *arg = new int(i);
        pthread_create(&workerTID[i], nullptr, worker_thread, arg);
    }

    managerLoop();

    pthread_join(dlh_tid, nullptr);
    for (int i = 0; i < N; ++i)
        pthread_join(workerTID[i], nullptr);

    cleanup();
    return 0;
}
