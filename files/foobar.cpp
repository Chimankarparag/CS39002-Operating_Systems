/* foobar.cpp – outermost wrapper; contains main() */
#include "global.cpp"
#include "manager.cpp"
#include "work.cpp"

void manager(int f, int b, const char *foofile, const char *barfile);

int main(int argc, char *argv[])
{
    if (argc != 5) {
        std::fprintf(stderr,
            "Usage: %s <f> <b> <foofile> <barfile>\n"
            "  f        = number of foocars to produce\n"
            "  b        = number of barcars to produce\n"
            "  foofile  = foocar specification file\n"
            "  barfile  = barcar specification file\n",
            argv[0]);
        return EXIT_FAILURE;
    }

    int f = std::atoi(argv[1]);
    int b = std::atoi(argv[2]);
    const char *foofile = argv[3];
    const char *barfile = argv[4];

    if (f <= 0 || b <= 0) {
        std::fprintf(stderr, "Error: f and b must be positive integers.\n");
        return EXIT_FAILURE;
    }

    manager(f, b, foofile, barfile);
    return EXIT_SUCCESS;
}
