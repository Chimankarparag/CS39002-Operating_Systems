/* genschedule.cpp – generate random car-schedule input files */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

void gensched(int n, int m, char *fname)
{
    FILE *fp = static_cast<FILE *>(std::fopen(fname, "w"));
    if (fp == nullptr) {
        std::fprintf(stderr, "*** Error in opening file %s\n", fname);
        std::exit(1);
    }

    std::fprintf(fp, "%d %d\n", n, m);
    for (int i = 0; i < n; ++i) {
        std::fprintf(fp, "%2d %2d", i, std::rand() % m);
        for (int j = i + 1; j < n; ++j)
            if (std::rand() % 4 == 0) std::fprintf(fp, " %2d", j);
        std::fprintf(fp, " -1\n");
    }

    std::fclose(fp);
}

int main(int argc, char *argv[])
{
    int n1 = 32, n2 = 40, m1 = 8, m2 = 10;
    char *fname, *bname;

    if (argc >= 2) n1 = std::atoi(argv[1]);
    if (argc >= 3) n2 = std::atoi(argv[2]);
    if (argc >= 4) m1 = std::atoi(argv[3]);
    if (argc >= 5) m2 = std::atoi(argv[4]);
    fname = (argc >= 6) ? argv[5] : strdup("fooschedule.txt");
    bname = (argc >= 7) ? argv[6] : strdup("barschedule.txt");

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    gensched(n1, m1, fname);
    gensched(n2, m2, bname);

    return 0;
}
