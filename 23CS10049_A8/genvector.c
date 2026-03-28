#include <stdlib.h>

int genallocationreq ( int TOT[], int ALLOC[], int REQ[], int m )
{
   int j, allowed, status = 0;

   for (j=0; j<m; ++j) {
      allowed = TOT[j] - ALLOC[j];
      if (allowed > 3) allowed = 3;
      if ( (allowed > 0) && (rand() % 2) ) {
         REQ[j] = rand() % (allowed + 1);
         if (REQ[j]) status = 1;
      } else {
         REQ[j] = 0;
      }
   }
   return status;
}

int genreleasereq ( int ALLOC[], int REL[], int m )
{
   int j, status = 0;

   for (j=0; j<m; ++j) {
      if ( (ALLOC[j] > 0) && (rand() % 2) ) {
         REL[j] = rand() % (1 + ALLOC[j]);
         if (REL[j]) status = 1;
      } else {
         REL[j] = 0;
      }
   }
   return status;
}
