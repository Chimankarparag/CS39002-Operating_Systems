#include "global.cpp" 
#include "work.cpp"
#include "manager.cpp"

int main (int argc, char* argv[]) {

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

    Foocar.read(argv[3], CarType::FOOCAR, "Foocar"); //init the name 
    Barcar.read(argv[4], CarType::BARCAR, "Barcar"); //init the name

    // Print Dependency, Prerequisite and Worker Assignment
    Foocar.print_info();
    Barcar.print_info();

    // Run the manager, which creates workers, schedules production, drives
    runManager(f, b);
    return 0;

}