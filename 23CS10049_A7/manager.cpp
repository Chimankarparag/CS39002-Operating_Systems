// It creates M worker threads,
// runs the production sequence, and waits for all workers to finish.
// exit when all the cares are produced and all workers have quit.
// predeclared func
void* workerFunc(void* arg);

static void buildSeq(int f, int b){
    int couple = min(f, b);
    for (int i = 0; i < couple; i++)
    {
        prodSeq.push_back(CarType::FOOCAR);
        prodSeq.push_back(CarType::BARCAR);
    }
    if (f > b)
        for (int i = b; i < f; i++) prodSeq.push_back(CarType::FOOCAR);
    else
        for (int i = f; i < b; i++) prodSeq.push_back(CarType::BARCAR);
    
}

void runManager(int f, int b){

    buildSeq(f,b);
    M_total = max(Foocar.M, Barcar.M);

    // set up barrier init
    pthread_barrier_init(&bop, nullptr, M_total + 1);
    pthread_barrier_init(&eop, nullptr, M_total + 1);
    pthread_mutex_init(&mtx, nullptr);

    // Allocate global arrays based on maximum possible sizes
    int max_N = max(Foocar.N, Barcar.N);
    PSTAT.resize(max_N,PStatus::PENDING);
    WSTAT.resize(M_total, WState::START);
    WINFO.resize(M_total,0);
    cnd.resize(M_total);

    for (int w = 0; w < M_total; w++) {
        pthread_cond_init(&cnd[w], nullptr);
    }

    vector<pthread_t> workers(M_total);
    vector<int>id(M_total);
    for (long w = 0; w < M_total; w++) {
        // id[w]=w;
        pthread_create(&workers[w], nullptr, workerFunc, (void*)w);
    }

    // Main Production Loop
    for (CarType type : prodSeq ){
        currCar = (type == CarType::FOOCAR) ? &Foocar : &Barcar;

        fill(PSTAT.begin(), PSTAT.end(), PStatus::PENDING);
        fill(WSTAT.begin(), WSTAT.end(), WState::START);
        
        printf("\n+++ Production of a %s begins\n",currCar->name.c_str());
        printWorker();

        pthread_barrier_wait(&bop); // start workers
        pthread_barrier_wait(&eop); // wait for workers to finish
        
    }

    allDone = true;
    printf("\n+++ ALL productions completed\n");
    printWorker();

    pthread_barrier_wait(&bop); // Release workers one last time so they can quit

    for(int w =0 ;w<M_total; w++){
        pthread_join(workers[w], nullptr);
    }
    pthread_mutex_destroy(&mtx);
    pthread_barrier_destroy(&bop);
    pthread_barrier_destroy(&eop);
    for (int w = 0; w < M_total; w++) {
        pthread_cond_destroy(&cnd[w]);
    }

}