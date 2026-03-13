// foobar.cpp
// Outermost wrapper that stitches together the three implementation files.
// Compile with:
//   g++ -Wall -std=c++17 -o manufacture foobar.cpp -pthread
//
// Usage:
//   ./manufacture <f> <b> <foocar-schedule> <barcar-schedule>
//     f  = number of foocar orders
//     b  = number of barcar orders
//
// The three #include directives below bring all definitions into a single
// translation unit in the order global → work → manager so that each file
// sees everything declared before it.

#include "global.cpp"   // shared types, globals, utility (print_col, etc.)
#include "work.cpp"     // worker_func — the thread entry point
#include "manager.cpp"  // run_manager — drives the bop/eop production cycle

int main(int argc, char* argv[]) {

    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s <f> <b> <foocar-file> <barcar-file>\n", argv[0]);
        return 1;
    }

    int f = atoi(argv[1]); // number of foocar orders
    int b = atoi(argv[2]); // number of barcar orders

    if (f < 0 || b < 0 || f + b == 0) {
        fprintf(stderr, "Error: f and b must be non-negative and f+b > 0.\n");
        return 1;
    }

    // Read and build both car specifications from their schedule files.
    foocar.read(argv[3], "Foocar");
    barcar.read(argv[4], "Barcar");

    // Print the full dependency/prerequisite/assignment tables so the
    // evaluator can verify the input was parsed correctly.
    foocar.print_info();
    barcar.print_info();

    // Hand control to the manager, which creates workers, schedules
    // production, drives the barrier synchronisation, and joins threads.
    run_manager(f, b);

    return 0;
}
