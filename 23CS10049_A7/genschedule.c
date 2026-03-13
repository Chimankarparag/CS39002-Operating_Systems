#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void gensched ( int n, int m, char *fname )
{
   FILE *fp;
   int i, j;

   fp = (FILE *)fopen(fname, "w");
   if (fp == NULL) {
      fprintf(stderr, "*** Error in opening file %s\n", fname);
      exit(1);
   }

   fprintf(fp, "%d %d\n", n, m);
   for (i=0; i<n; ++i) {
      fprintf(fp, "%2d %2d", i, rand() % m);
      for (j=i+1; j<n; ++j) if (rand() % 4 == 0) fprintf(fp, " %2d", j);
      fprintf(fp, " -1\n");
   }

   fclose(fp);
}

int main ( int argc, char *argv[] )
{
   int n1 = 32, n2 = 40, m1 = 8, m2 = 10;
   char *fname, *bname;

   if (argc >= 2) n1 = atoi(argv[1]);
   if (argc >= 3) n2 = atoi(argv[2]);
   if (argc >= 4) m1 = atoi(argv[3]);
   if (argc >= 5) m2 = atoi(argv[4]);
   fname = (argc >= 6) ? argv[5] : strdup("fooschedule.txt");
   bname = (argc >= 7) ? argv[6] : strdup("barschedule.txt");

   srand((unsigned int)time(NULL));

   gensched(n1, m1, fname);
   gensched(n2, m2, bname);

   exit(0);
}
