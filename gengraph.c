#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main ( int argc, char *argv[] )
{
   int n, i, j;
   double p;
   FILE *fp;

   if (argc >= 3) { n = atoi(argv[1]); p = atof(argv[2]); }
   else { n = 10; p = 0.5; }

   srand((unsigned int)time(NULL));

   fp = (FILE *)fopen("graph.txt", "w");
   fprintf(fp, "%d\n", n);
   for (i=1; i<=n; ++i) {
      fprintf(fp, "%d ->", i);
      for (j=1; j<=n; ++j) {
         if (j != i) {
            if ((double)rand() / (double)RAND_MAX <= p) fprintf(fp, " %d", j);
         }
      }
      fprintf(fp, "\n");
   }
   fclose(fp);
   exit(0);
}
