#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DFTPROCNO 16
#define FILENAME "bursts.txt"

int main ( int argc, char *argv[] )
{
   int n, i, j, t;
   FILE *fp;

   n = (argc > 1) ? atoi(argv[1]) : DFTPROCNO;
   srand((unsigned int)time(NULL));
   fp = (FILE *)fopen(FILENAME, "w");
   t = 0;
   for (i=0; i<n; ++i) {
      fprintf(fp, "%-6d %d ", t, rand() % 3);
      for (j=0; j<10; ++j) {
         fprintf(fp, "%2d %2d ", 2 + rand() % 19, 20 + rand() % 11);
      }
      fprintf(fp, "%2d\n", 2 + rand() % 19);
      t += 10 + rand() % 31;
   }
   fprintf(fp, "-1\n");
   fclose(fp);
   exit(0);
}
