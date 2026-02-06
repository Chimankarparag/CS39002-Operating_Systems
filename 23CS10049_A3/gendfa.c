#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main ( int argc, char *argv[] )
{
   int s, n, i, j, r, N, p, c;
   char *fname;
   FILE *fp;
   int **delta, *isfinal;

   if (argc == 1) { s = 4; n = 20; fname = strdup("dfa.txt"); }
   else if (argc == 2) { s = atoi(argv[1]); n = 20; fname = strdup("dfa.txt"); }
   else if (argc == 3) { s = atoi(argv[1]); n = atoi(argv[2]); fname = strdup("dfa.txt"); }
   else { s = atoi(argv[1]); n = atoi(argv[2]); fname = argv[3]; }

   srand((unsigned int)time(NULL));

   isfinal = (int *)malloc(n * sizeof(int));
   delta = (int **)malloc(n * sizeof(int *));
   for (i=0; i<n; ++i) {
      delta[i] = (int *)malloc(s * sizeof(int));
      for (j=0; j<s; ++j) delta[i][j] = -1;
   }

   p = c = 0; N = n - 1;
   while (N > 0) {
      j = 1 + rand() % 4;
      if (j > N) j = N;
      N -= j;
      for (i=0; i<j; ++i) {
         do r = rand() % s; while (delta[p][r] != -1);
         ++c; delta[p][r] = c;
      }
      ++p;
   }

   for (i=0; i<n; ++i) {
      for (j=0; j<s; ++j) {
         if (delta[i][j] == -1) {
            r = rand() % n;
            delta[i][j] = r;
         }
      }
      r = rand() % 3;
      isfinal[i] = (r == 0) ? 1 : 0;
   }

   fp = (FILE *)fopen(fname, "w");
   fprintf(fp, "%d\n%d\n", s, n);
   for (i=0; i<n; ++i) {
      fprintf(fp, "%d %c", i, (isfinal[i]) ? 'F' : 'N');
      for (j=0; j<s; ++j) fprintf(fp, " %d", delta[i][j]);
      fprintf(fp, "\n");
   }
   fclose(fp);

   for (i=0; i<n; ++i) free(delta[i]);
   free(delta); free(isfinal);

   exit(0);
}
