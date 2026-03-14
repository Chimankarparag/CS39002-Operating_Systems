// each thread when created is passed with a value w , worker number 

// wait on bop -> work -> then wait on eop for all
// access the global workerAsgn for the Car,
// check the cond_variable, sleep or work
// assign the work and update PSTAT when done
// WINFO and WSTAT arrays

// once done call for exit
void* workerFunc(void *arg){
    int w = (long)arg;

    while(true){
        // wait for all to join
        pthread_barrier_wait(&bop);

        if(allDone){
            pthread_mutex_lock(&mtx);
            printCol(w," Quit");
            pthread_mutex_unlock(&mtx);
            pthread_exit(nullptr);
        }
  
        // if this worker is not involved, ie w > blacar.M
        // print all done and update status
        // currCar set to Global object Blacar in manager before &bop
        bool involved = (w < currCar->M) && (!currCar->workerAsgn[w].empty());

        if(!involved){
            pthread_mutex_lock(&mtx);
            WSTAT[w]= WState::DONE;
            printCol(w,"All Done");
            pthread_mutex_unlock(&mtx);

        }else{
            const vector<int>&workerJob = currCar->workerAsgn[w];
            
            for(int p : workerJob){
                pthread_mutex_lock(&mtx);
                // in manager init to state start
                WSTAT[w]= WState::WORKING;
                WINFO[w]=p; // worker w is working on part p

                // if p has prereq left(state waiting), else execute and do next p
                if(!prereqsDone(p)){
                    WSTAT[w]= WState::WAITING;
                    printCol(w, "Wait " + to_string(p));
                
                    // The while-loop guards against spurious wakeups:
                    // pthread_cond_wait may return even without a signal,
                    // so we always re-check the condition.
                    while(!prereqsDone(p)) pthread_cond_wait(&cnd[w], &mtx);

                    // other worker needs to do the pre-req parts and this worker sleeps
                    // the last remaining pre req wakes this w
                    WSTAT[w] = WState::WORKING;
                }
                // cond wait regains mtx

                // complete the part p
                printCol(w, "Part "+ to_string(p));
                // update that the part is done
                PSTAT[p] = PStatus::DONE;

                // wake up the worker on the part q which had prerequisite p
                for(int q : currCar->deps[p])
                {
                    // wake that worker only when all prereq of q are done
                    if(prereqsDone(q)){
                        int nextWorker = currCar->whichWorker[q];
                        // ensure next worker was waiting only on q
                        // it may happen that same worker is waiting on q_before, he will later do q
                        // but that q_before has prereq left
                        // so we might miss-wake him for q, and he starts working q_before
                        if(WSTAT[nextWorker] == WState::WAITING && WINFO[nextWorker]== q){
                            printCol(w, "Wake up");
                            pthread_cond_signal(&cnd[nextWorker]);                            
                        }
                    }
                }
                // release mtx
                pthread_mutex_unlock(&mtx);

            }

            pthread_mutex_lock(&mtx);
            WSTAT[w] = WState::DONE;
            printCol(w, "All done");
            pthread_mutex_unlock(&mtx);
        }
        pthread_barrier_wait(&eop);
    }
    return nullptr;
}