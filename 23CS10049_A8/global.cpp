#include "global.h"

using namespace std;

int N;
int M;
int R;

vector<int> TOTAL;
vector<int> AVAILABLE;
vector<vector<int>> ALLOCATION;
vector<vector<int>> REQUEST;
vector<vector<int>> RELMAT;

int REQFROM = -1;
ReqType REQTYPE = ReqType::NONE;

int NACTIVE;
vector<WorkerStatus> STATUS;
vector<pthread_t> workerTID;
vector<bool> GRANTED;

queue<int> RQ;

void rqRem(int id) {
    queue<int> tmp;
    bool removed = false;
    while (!RQ.empty()) {
        int front = RQ.front();
        RQ.pop();
        if (!removed && front == id) {
            removed = true;
        } else {
            tmp.push(front);
        }
    }
    RQ = move(tmp);
}

vector<int> rqSnap() {
    vector<int> snap;
    snap.reserve(RQ.size());
    queue<int> tmp = RQ;
    while (!tmp.empty()) {
        snap.push_back(tmp.front());
        tmp.pop();
    }
    return snap;
}

pthread_mutex_t RMTX;
pthread_cond_t SCND;
pthread_mutex_t SMTX;
pthread_cond_t ACND;
pthread_mutex_t AMTX;
vector<pthread_cond_t>  WCND;
vector<pthread_mutex_t> WMTX;

void start() {
    TOTAL.assign(M, 0);
    AVAILABLE.assign(M, 0);

    ALLOCATION.assign(N, vector<int>(M, 0));
    REQUEST.assign(N, vector<int>(M, 0));
    RELMAT.assign(N, vector<int>(M, 0));

    STATUS.assign(N, WorkerStatus::ACTIVE);
    workerTID.resize(N);
    GRANTED.assign(N, false);

    NACTIVE = N;

    for (int j = 0; j < M; ++j)
        TOTAL[j] = AVAILABLE[j] = 8 + rand() % 25;

    pthread_mutex_init(&RMTX, nullptr);
    pthread_mutex_init(&SMTX, nullptr);
    pthread_mutex_init(&AMTX, nullptr);
    pthread_cond_init(&SCND, nullptr);
    pthread_cond_init(&ACND, nullptr);

    WCND.resize(N);
    WMTX.resize(N);
    for (int i = 0; i < N; ++i) {
        pthread_cond_init (&WCND[i], nullptr);
        pthread_mutex_init(&WMTX[i], nullptr);
    }
}

void cleanup() {
    for (int i = 0; i < N; ++i) {
        pthread_cond_destroy (&WCND[i]);
        pthread_mutex_destroy(&WMTX[i]);
    }
    pthread_mutex_destroy(&RMTX);
    pthread_mutex_destroy(&SMTX);
    pthread_mutex_destroy(&AMTX);
    pthread_cond_destroy (&SCND);
    pthread_cond_destroy (&ACND);
}

bool generate_request(int who, vector<int> &req) {
    req.assign(M, 0);
    bool status = false;
    for (int j = 0; j < M; ++j) {
        int allowed = TOTAL[j] - ALLOCATION[who][j];
        if (allowed > 3) allowed = 3;
        if (allowed > 0 && rand() % 2) {
            req[j] = rand() % (allowed + 1);
            if (req[j]) status = true;
        } else {
            req[j] = 0;
        }
    }
    return status;
}

bool generate_release(int who, vector<int> &rel) {
    rel.assign(M, 0);
    bool status = false;
    for (int j = 0; j < M; ++j) {
        if (ALLOCATION[who][j] > 0 && rand() % 2) {
            rel[j] = rand() % (ALLOCATION[who][j] + 1);
            if (rel[j]) status = true;
        } else {
            rel[j] = 0;
        }
    }
    return status;
}
