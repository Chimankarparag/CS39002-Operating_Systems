#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NFRAMES 12288
#define PTSIZE 2048

typedef struct {
   unsigned short int *list;
   unsigned short int *hist;
} ptentry;

typedef struct {
   int fno;
   int pno;
   int owner;
} ffentry;

typedef struct _node {
   int item;
   struct _node *next;
} node;

typedef struct {
   node *F;
   node *B;
} queue;

typedef struct {
   int n;
   int m;
   int *size;
   int **sidx;
   int *scnt;
} simdata;

typedef struct {
   /* System-wide data structures */
   int nff;               /* Number of free frames */
   int NFFMIN;            /* Minimum number of free frames */
   ffentry *fflist;       /* Array of free frames */
   queue readylist;       /* Ready queue for round-robin scheduling */
   /* Per-process data structures */
   ptentry *pagetable;    /* Page table of the process */
   /* For statistical purpose */
   int *npageaccess;      /* Number of page accesses made by the process */
   int *npagefault;       /* Number of page faults encountered by the process */
   int *npagereplacement; /* Number of page replacements encountered by the process */
   int *nattempt1;        /* Number of loads avoided during page replacements */
   int *nattempt2;        /* Number of no-owner pages during page replacements */
   int *nattempt3;        /* Number of own pages during page replacements */
   int *nattempt4;        /* Number of other-owner pages during page replacements */
} kerneldata;

queue initQ ( )
{
   return (queue){NULL, NULL};
}

int emptyQ ( queue Q )
{
   return (Q.F == NULL);
}

int front ( queue Q )
{
   if (Q.F == NULL) return -1;
   return Q.F -> item;
}

queue enQ ( queue Q, int x )
{
   node *p;

   p = (node *)malloc(sizeof(node));
   p -> item = x;
   p -> next = NULL;
   if (Q.F == NULL) {
      Q.F = Q.B = p;
   } else {
      Q.B -> next = p;
      Q.B = p;
   }
   return Q;
}

queue deQ ( queue Q )
{
   node *p;

   if (Q.F == NULL) return Q;
   p = Q.F;
   Q.F = Q.F -> next;
   if (Q.F == NULL) Q.B = NULL;
   free(p);
   return Q;
}

simdata readsimdata ( )
{
   FILE *fp;
   int n, m;
   simdata S;
   int i, j;

   fp = (FILE *)fopen("search.txt", "r");
   fscanf(fp, "%d%d", &n, &m);
   S.n = n;
   S.m = m;
   S.size = (int *)malloc(n * sizeof(int));
   S.sidx = (int **)malloc(n * sizeof(int *));
   S.scnt = (int *)malloc(n * sizeof(int));
   for (i=0; i<n; ++i) {
      fscanf(fp, "%d", S.size + i);
      S.sidx[i] = (int *)malloc(m * sizeof(int));
      for (j=0; j<m; ++j) fscanf(fp, "%d", S.sidx[i]+j);
      S.scnt[i] = 0;
   }
   fclose(fp);
   return S;
}

kerneldata initkerneldata ( simdata SD )
{
   kerneldata KD;
   int i, j, f, n;

   n = SD.n;

   /* Initialize global data structures */

   /* Frame-related data structures */
   KD.nff = 0;
   KD.NFFMIN = 1000;
   KD.fflist = (ffentry *)malloc(NFRAMES * sizeof(ffentry));
   for (f=0; f<NFRAMES; ++f) {
      KD.fflist[f].fno = NFRAMES - 1 - f;
      KD.fflist[f].pno = -1;
      KD.fflist[f].owner = -1;
      ++(KD.nff);
   }
   
   /* Ready queue */
   KD.readylist = initQ();

   /* Initialize per-process data structures */

   KD.pagetable = (ptentry *)malloc(n * sizeof(ptentry));
   KD.npageaccess = (int *)malloc(n * sizeof(int));
   KD.npagefault = (int *)malloc(n * sizeof(int));
   KD.npagereplacement = (int *)malloc(n * sizeof(int));
   KD.nattempt1 = (int *)malloc(n * sizeof(int));
   KD.nattempt2 = (int *)malloc(n * sizeof(int));
   KD.nattempt3 = (int *)malloc(n * sizeof(int));
   KD.nattempt4 = (int *)malloc(n * sizeof(int));
   for (i=0; i<n; ++i) {
      KD.pagetable[i].list = (unsigned short int *)malloc(PTSIZE * sizeof(unsigned short int));
      KD.pagetable[i].hist = (unsigned short int *)malloc(PTSIZE * sizeof(unsigned short int));
      for (j=0; j<10; ++j) {
         --(KD.nff);
         KD.pagetable[i].list[j] = KD.fflist[KD.nff].fno | (3U << 14);
         KD.pagetable[i].hist[j] = 0xffff;
      }
      for (j=10; j<PTSIZE; ++j) KD.pagetable[i].list[j] = KD.pagetable[i].hist[j] = 0;
      KD.npageaccess[i] = KD.npagefault[i] = KD.npagereplacement[i] = 0;
      KD.nattempt1[i] = KD.nattempt2[i] = KD.nattempt3[i] = KD.nattempt4[i] = 0;
      KD.readylist = enQ(KD.readylist,i);
   }

   return KD;
}

kerneldata bsloop ( simdata SD, kerneldata KD, int i, int k, int size )
{
   int L, R, M, j;
   int p, q, f, minhist, pframe, qframe;

   L = 0; R = size - 1;
   while (L < R) {
      M = (L + R) / 2;
      ++(KD.npageaccess[i]);  /* Book keeping */
      p = (M >> 10) + 10;     /* Logical page number */
      if ((KD.pagetable[i].list[p] & (1U << 15)) == 0) { /* Page not loaded */
         #ifdef VERBOSE
         printf("    Fault on Page %4d:", p);
         #endif
         ++(KD.npagefault[i]);
         if (KD.nff > KD.NFFMIN) {   /* Enough free frames available */
            --(KD.nff);
            pframe = KD.fflist[KD.nff].fno;
            KD.pagetable[i].list[p] = pframe | (1U << 15);
            KD.pagetable[i].hist[p] = 0xffff;
            #ifdef VERBOSE
            printf(" Free frame %d found\n", pframe);
            #endif
         } else {                    /* Page replacement needed */
            ++(KD.npagereplacement[i]);

            /* Find the victim page (smallest history) */
            minhist = 1000000000; q = -1;
            for (j=10; j<PTSIZE; ++j) {
               if ( (KD.pagetable[i].list[j] & (1U << 15)) && (KD.pagetable[i].hist[j] < minhist) ) {
                  minhist = KD.pagetable[i].hist[j];
                  q = j;
               }
            }
            qframe = KD.pagetable[i].list[q] &= 0x3fff;
            #ifdef VERBOSE
            printf(" To replace Page %d at Frame %d [history = %d]\n", q, qframe, minhist);
            #endif

            /* Locate a frame in the free-frame list */

            /* Attempt 1 */
            for (f=0; f<KD.nff; ++f) {
               if ((KD.fflist[f].owner == i) && (KD.fflist[f].pno == p)) {
                  #ifdef VERBOSE
                  printf("        Attempt 1: Page found in free frame %d\n", KD.fflist[f].fno);
                  #endif
                  ++(KD.nattempt1[i]);
                  break;
               }
            }

            /* Attempt 2 */
            if (f == KD.nff) {
               for (f=0; f<KD.nff; ++f) {
                  if (KD.fflist[f].owner == -1) {
                     #ifdef VERBOSE
                     printf("        Attempt 2: Free frame %d owned by no process found\n", KD.fflist[f].fno);
                     #endif
                     ++(KD.nattempt2[i]);
                     break;
                  }
               }
            }

            /* Attempt 3 */
            if (f == KD.nff) {
               for (f=0; f<KD.nff; ++f) {
                  if (KD.fflist[f].owner == i) {
                     #ifdef VERBOSE
                     printf("        Attempt 3: Own page %d found in free frame %d\n", KD.fflist[f].pno, KD.fflist[f].fno);
                     #endif
                     ++(KD.nattempt3[i]);
                     break;
                  }
               }
            }

            /* Attempt 4 */
            if (f == KD.nff) {
               f = rand() % KD.nff;
               #ifdef VERBOSE
               printf("        Attempt 4: Free frame %d owned by Process %d chosen\n", KD.fflist[f].fno, KD.fflist[f].owner);
               #endif
               ++(KD.nattempt4[i]);
            }

            pframe = KD.fflist[f].fno;
            KD.fflist[f].fno = qframe;
            KD.fflist[f].pno = q;
            KD.fflist[f].owner = i;
            KD.pagetable[i].list[p] = pframe | (1U << 15);
            KD.pagetable[i].hist[p] = 0xffff;
            KD.pagetable[i].list[q] = 0;
            KD.pagetable[i].hist[q] = 0;
         }
      }
      KD.pagetable[i].list[p] |= (1U << 14);   /* Set reference bit */
      if (k <= M) R = M;
      else L = M + 1;
   }

   /* Update page-reference history for process i */
   for (j=10; j<PTSIZE; ++j) {
      if (KD.pagetable[i].list[j] & (1U << 15)) {       /* if page is valid */
         KD.pagetable[i].hist[j] >>= 1;
         if ((KD.pagetable[i].list[j]) & (1U << 14)) {  /* if reference bit is set */
            KD.pagetable[i].hist[j] |= (1U << 15);      /* store 1 in msb of history */
            KD.pagetable[i].list[j] ^= (1U << 14);      /* clear reference bits */
         }
      }
   }

   ++(SD.scnt[i]);
   if (SD.scnt[i] < SD.m) {
      KD.readylist = enQ(KD.readylist, i);
   } else {
      /* Return all frames occupied by process i to system pool */
      for (j=0; j<PTSIZE; ++j) {
         if (KD.pagetable[i].list[j] & (1U << 15)) {    /* if page is valid */
            KD.fflist[KD.nff].fno = (KD.pagetable[i].list[j] & 0x3fff);
            KD.fflist[KD.nff].pno = -1;
            KD.fflist[KD.nff].owner = -1;
            ++(KD.nff);
         }
      }
   }

   return KD;
}

kerneldata nextsearch ( simdata SD, kerneldata KD )
{
   int i, j, k;

   i = front(KD.readylist);
   KD.readylist = deQ(KD.readylist);
   j = SD.scnt[i];
   k = SD.sidx[i][j];
   #ifdef VERBOSE
   printf("+++ Process %d: Search %d\n", i, j+1);
   #endif
   KD = bsloop(SD, KD, i, k, SD.size[i]);
   
   return KD;
}

void printstat ( kerneldata KD, int n )
{
   int i, pa, pf, pr, na1, na2, na3, na4;
   int tpa = 0, tpf = 0, tpr = 0, tna1 = 0, tna2 = 0, tna3 = 0, tna4 = 0;

   printf("+++ Page access summary\n");
   printf("    PID     Accesses        Faults         Replacements                        Attempts\n");
   for (i=0; i<n; ++i) {
      pa = KD.npageaccess[i]; tpa += pa;
      pf = KD.npagefault[i]; tpf += pf;
      pr = KD.npagereplacement[i]; tpr += pr;
      na1 = KD.nattempt1[i]; tna1 += na1;
      na2 = KD.nattempt2[i]; tna2 += na2;
      na3 = KD.nattempt3[i]; tna3 += na3;
      na4 = KD.nattempt4[i]; tna4 += na4;
      printf("    %-6d    %-6d    %-5d (%5.2lf%%)    %-5d (%5.2lf%%)    ",
             i, pa,
             pf, 100 * (double)pf / (double)pa,
             pr, 100 * (double)pr / (double)pa);
      printf("%3d + %3d + %3d + %3d  (%.2lf%% + %.2lf%% + %.2lf%% + %.2lf%%)\n",
             na1, na2, na3, na4,
             100 * (double)na1 / (double)pr,
             100 * (double)na2 / (double)pr,
             100 * (double)na3 / (double)pr,
             100 * (double)na4 / (double)pr);
   }
   printf("\n    Total     %-6d    %-5d (%5.2lf%%)    %-5d (%5.2lf%%)    ",
          tpa,
          tpf, 100 * (double)tpf / (double)tpa,
          tpr, 100. * (double)tpr / (double)tpa);
   printf("%d + %d + %d + %d  (%.2lf%% + %.2lf%% + %.2lf%% + %.2lf%%)\n",
          tna1, tna2, tna3, tna4,
          100 * (double)tna1 / (double)tpr,
          100 * (double)tna2 / (double)tpr,
          100 * (double)tna3 / (double)tpr,
          100 * (double)tna4 / (double)tpr);
}

int main ()
{
   simdata SD;
   kerneldata KD;

   srand((unsigned int)time(NULL));
   // srand(123456789);

   SD = readsimdata();

   KD = initkerneldata(SD);

   while (!emptyQ(KD.readylist)) KD = nextsearch(SD,KD);

   printstat(KD,SD.n);

   exit(0);
}
