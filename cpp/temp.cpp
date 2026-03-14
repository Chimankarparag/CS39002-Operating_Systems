// manager.cpp
void manager_thread(int f_count, int b_count, const string& foo_file, const string& bar_file) {
    CarData foocar, barcar;
    foocar.load_from_file(foo_file, CarType::FOOCAR, "foocar");
    barcar.load_from_file(bar_file, CarType::BARCAR, "barcar");

    M_total = max(foocar.M, barcar.M); [cite: 23]

    // Create the interleaved production sequence [cite: 6, 8]
    while (f_count > 0 || b_count > 0) {
        if (f_count > 0) { prod_seq.push_back(CarType::FOOCAR); f_count--; }
        if (b_count > 0) { prod_seq.push_back(CarType::BARCAR); b_count--; }
    }

    // Allocate global arrays based on maximum possible sizes
    int max_N = max(foocar.N, barcar.N);
    PSTAT.resize(max_N);
    WSTAT.resize(M_total);
    WINFO.resize(M_total);
    cnd.resize(M_total);

    pthread_mutex_init(&mtx, nullptr);
    pthread_barrier_init(&bop, nullptr, M_total + 1);
    pthread_barrier_init(&eop, nullptr, M_total + 1);

    for (int w = 0; w < M_total; w++) {
        pthread_cond_init(&cnd[w], nullptr);
    }

    vector<pthread_t> workers(M_total);
    for (long w = 0; w < M_total; w++) {
        pthread_create(&workers[w], nullptr, worker_thread, (void*)w); [cite: 40]
    }

    // Run production sequence
    for (CarType type : prod_seq) {
        cur_car = (type == CarType::FOOCAR) ? &foocar : &barcar;

        // Initialize status arrays for the new car [cite: 48]
        fill(PSTAT.begin(), PSTAT.end(), PStatus::PENDING);
        fill(WSTAT.begin(), WSTAT.end(), WState::START);

        printf("\n+++ Production of a %s begins\n", cur_car->name.c_str());
        print_header();

        pthread_barrier_wait(&bop); // Start workers
        pthread_barrier_wait(&eop); // Wait for workers to finish
    }

    // End of all productions [cite: 29]
    all_done_flag = true;
    printf("\n+++ All productions completed\n");
    print_header();
    
    pthread_barrier_wait(&bop); // Release workers one last time so they can quit

    for (int w = 0; w < M_total; w++) {
        pthread_join(workers[w], nullptr); [cite: 65]
    }

    pthread_mutex_destroy(&mtx);
    pthread_barrier_destroy(&bop);
    pthread_barrier_destroy(&eop);
    for (int w = 0; w < M_total; w++) {
        pthread_cond_destroy(&cnd[w]);
    }
}