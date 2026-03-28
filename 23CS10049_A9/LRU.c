// 23CS10049 Parag Mahadeo Chimankar
// Assignment 9

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROWS 1080
#define COLS 1920
#define PAGE_SIZE 4096 //4KB
#define PTR_SIZE 8      
#define MAXT_PT_SIZE 5000
#define PIXEL_SIZE  4
// time value of nanosecomds
#define PAGEFAULT_TIME 10000000.0  
#define MEMACESS_TIME 100.0       
#define NS_PER_SEC 1e9

#define SZ_CHAR_ARRAY ((long long)ROWS * COLS)           
#define SZ_CHAR3_ARRAY ((long long)ROWS * COLS * 3)       
#define SZ_PIXEL_ARRAY ((long long)ROWS * COLS * PIXEL_SIZE) 
#define SZ_PTR_BLOCK ((long long)ROWS * PTR_SIZE)     

typedef struct Node {
    int        page_id;
    struct Node *p;
    struct Node *n;
}Node;

typedef struct {
    Node  *PT[MAXT_PT_SIZE]; 
    Node  *front;          
    Node  *last;           
    int    loaded;        
    int    f;             
    long long pageFaults;
    long long totalAccess;
}LRU;

static void init(LRU *lru, int f)
{
    memset(lru->PT, 0, sizeof(lru->PT));
    lru->front        = NULL;
    lru->last         = NULL;
    lru->loaded       = 0;
    lru->f            = f;
    lru->pageFaults  = 0;
    lru->totalAccess = 0;
}

static inline void moveFront(LRU *lru, Node *node)
{
    if (node == lru->front) return;

    if (node->p) node->p->n = node->n;
    if (node->n) node->n->p = node->p;
    if (node == lru->last) lru->last = node->p;

    node->p = NULL;
    node->n = lru->front;
    if (lru->front) lru->front->p = node;
    lru->front = node;
}

static void accessPage(LRU *lru, int p)
{
    lru->totalAccess++;

    if (lru->PT[p] != NULL) {
        moveFront(lru, lru->PT[p]);
        return;
    }
    // in case of page fault
    lru->pageFaults++;

    Node *node = (Node *)malloc(sizeof(Node));
    node->page_id = p;
    node->p    = NULL;
    node->n    = NULL;

    if (lru->loaded < lru->f) {
        lru->loaded++;
    } else {
        Node *victim = lru->last;
        lru->PT[victim->page_id] = NULL;

        if (victim->p) victim->p->n = NULL;
        lru->last = victim->p;
        if (lru->last == NULL) lru->front = NULL;
        free(victim);
    }

    node->n = lru->front;
    if (lru->front) lru->front->p = node;
    lru->front = node;
    if (lru->last == NULL) lru->last = node; 

    lru->PT[p] = node;
}

static void freeLRU(LRU *lru)
{
    Node *curr = lru->front;
    while(curr){
        Node *next = curr->n;
        free(curr);
        curr = next;
    }
    lru->front = lru->last = NULL;
}

static inline int pageOf(long long byteOffset)
{
    return (int)(byteOffset / PAGE_SIZE);
}

// Schemes
static void s1(int k, int f)
{
    LRU lru;
    init(&lru, f);
    long long R = 0;
    long long G = R + SZ_CHAR_ARRAY;
    long long B = G + SZ_CHAR_ARRAY;
    long long r = B + SZ_CHAR_ARRAY;
    long long g = r + SZ_CHAR_ARRAY;

    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            int rowstart = (i - k < 0)? 0 : i - k;
            int rowend = (i + k >= ROWS)? ROWS-1 : i + k;
            int colstart = (j - k < 0)    ? 0    : j - k;
            int colend   = (j + k >= COLS)? COLS-1: j + k;

            /* Inner blurring loop: read input pixels */
            for (int row = rowstart; row <= rowend; ++row) {
                for (int col = colstart; col <= colend; ++col) {
                    int dx = row - i, dy = col - j;
                    if (dx*dx + dy*dy <= k*k) {
                        long long offset = (long long)row * COLS + col;
                        accessPage(&lru, pageOf(R + offset));
                        accessPage(&lru, pageOf(G + offset));
                        accessPage(&lru, pageOf(B + offset));
                    }
                }
            }

            long long outOffset = (long long)i * COLS + j;
            accessPage(&lru, pageOf(r + outOffset));
            accessPage(&lru, pageOf(g + outOffset));
            accessPage(&lru, pageOf(g + SZ_CHAR_ARRAY + outOffset));
        }
    }

    printf("+++ Scheme 1: Three static arrays\n");
    printf("    Total number of memory accesses = %lld\n", lru.totalAccess);
    printf("    Total number of page faults = %lld\n", lru.pageFaults);
    printf("    Page fault rate (percentage) = %.2f\n", 100.0 * lru.pageFaults / lru.totalAccess);

    double time_ns = (lru.totalAccess - lru.pageFaults) * MEMACESS_TIME + lru.pageFaults * PAGEFAULT_TIME;
    printf("    Total memory access time = %.2f sec\n\n", time_ns / NS_PER_SEC);
    freeLRU(&lru);
}

static void s2(int k, int f)
{
    LRU lru;
    init(&lru, f);

    long long RGB = 0;
    long long rgb = SZ_CHAR3_ARRAY;

    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            int rowstart = (i - k < 0)? 0 : i - k;
            int rowend = (i + k >= ROWS)? ROWS-1 : i + k;
            int colstart = (j - k < 0)    ? 0    : j - k;
            int colend   = (j + k >= COLS)? COLS-1: j + k;

            for (int row = rowstart; row <= rowend; ++row) {
                for (int col = colstart; col <= colend; ++col) {
                    int dx = row - i, dy = col - j;
                    if (dx*dx + dy*dy <= k*k) {
                        long long offset = (long long)row * COLS * 3 + (long long)col * 3;
                        accessPage(&lru, pageOf(RGB + offset));
                        accessPage(&lru, pageOf(RGB + offset + 1));
                        accessPage(&lru, pageOf(RGB + offset + 2));
                    }
                }
            }
            long long outOffset = (long long)i * COLS * 3 + (long long)j * 3;
            accessPage(&lru, pageOf(rgb + outOffset));
            accessPage(&lru, pageOf(rgb + outOffset + 1));
            accessPage(&lru, pageOf(rgb + outOffset + 2));
        }
    }

    printf("+++ Scheme 2: One static array of char\n");
    printf("    Total number of memory accesses = %lld\n", lru.totalAccess);
    printf("    Total number of page faults = %lld\n", lru.pageFaults);
    printf("    Page fault rate (percentage) = %.2f\n", 100.0 * lru.pageFaults / lru.totalAccess);

    double time_ns = (lru.totalAccess - lru.pageFaults) * MEMACESS_TIME + lru.pageFaults * PAGEFAULT_TIME;
    printf("    Total memory access time = %.2f sec\n\n", time_ns / NS_PER_SEC);
    freeLRU(&lru);
}

static void s3(int k, int f)
{
    LRU lru;
    lru_init(&lru, f);

    long long RGB = 0;
    long long rgb = SZ_PIXEL_ARRAY;

    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            int rowstart = (i - k < 0)? 0 : i - k;
            int rowend = (i + k >= ROWS)? ROWS-1 : i + k;
            int colstart = (j - k < 0)    ? 0    : j - k;
            int colend   = (j + k >= COLS)? COLS-1: j + k;

            for (int row = rowstart; row <= rowend; ++row) {
                for (int col = colstart; col <= colend; ++col) {
                    int dx = row - i, dy = col - j;
                    if (dx*dx + dy*dy <= k*k) {
                        long long offset = ((long long)row * COLS + col) * PIXEL_SIZE;
                        accessPage(&lru, pageOf(RGB + offset));
                    }
                }
            }

            long long outOffset = ((long long)i * COLS + j) * PIXEL_SIZE;
            accessPage(&lru, pageOf(rgb + outOffset));
        }
    }

    printf("+++ Scheme 3: One static array of struct\n");
    printf("    Total number of memory accesses = %lld\n", lru.totalAccess);
    printf("    Total number of page faults = %lld\n", lru.pageFaults);
    printf("    Page fault rate (percentage) = %.2f\n", 100.0 * lru.pageFaults / lru.totalAccess);

    double time_ns = (lru.totalAccess - lru.pageFaults) * MEMACESS_TIME + lru.pageFaults * PAGEFAULT_TIME;
    printf("    Total memory access time = %.2f sec\n\n", time_ns / NS_PER_SEC);
    freeLRU(&lru);
}

static void s4(int k, int f)
{
    LRU lru;
    lru_init(&lru, f);

    long long RP = 0;
    long long R  = RP + SZ_PTR_BLOCK;
    long long GP = R  + SZ_CHAR_ARRAY;
    long long G  = GP + SZ_PTR_BLOCK;
    long long BP = G  + SZ_CHAR_ARRAY;
    long long B  = BP + SZ_PTR_BLOCK;
    long long rp = B  + SZ_CHAR_ARRAY;
    long long r  = rp + SZ_PTR_BLOCK;
    long long gp = r  + SZ_CHAR_ARRAY;
    long long g  = gp + SZ_PTR_BLOCK;
    long long bp = g  + SZ_CHAR_ARRAY;
    long long b  = bp + SZ_PTR_BLOCK;

    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            int rowstart = (i - k < 0)? 0 : i - k;
            int rowend = (i + k >= ROWS)? ROWS-1 : i + k;
            int colstart = (j - k < 0)    ? 0    : j - k;
            int colend   = (j + k >= COLS)? COLS-1: j + k;

            for (int row = rowstart; row <= rowend; ++row) {
                for (int col = colstart; col <= colend; ++col) {
                    int dx = row - i, dy = col - j;
                    if (dx*dx + dy*dy <= k*k) {
                        long long ptrOffset = (long long)row * PTR_SIZE;  
                        long long dat_off = (long long)row * COLS + col; 

                        accessPage(&lru, pageOf(RP + ptrOffset));
                        accessPage(&lru, pageOf(R  + dat_off));
                        accessPage(&lru, pageOf(GP + ptrOffset));
                        accessPage(&lru, pageOf(G  + dat_off));
                        accessPage(&lru, pageOf(BP + ptrOffset));
                        accessPage(&lru, pageOf(B  + dat_off));
                    }
                }
            }

            long long ptrOffset = (long long)i * PTR_SIZE;
            long long dat_off = (long long)i * COLS + j;

            accessPage(&lru, pageOf(rp + ptrOffset));
            accessPage(&lru, pageOf(r  + dat_off));
            accessPage(&lru, pageOf(gp + ptrOffset));
            accessPage(&lru, pageOf(g  + dat_off));
            accessPage(&lru, pageOf(bp + ptrOffset));
            accessPage(&lru, pageOf(b  + dat_off));
        }
    }

    printf("+++ Scheme 4: Three dynamic arrays\n");
    printf("    Total number of memory accesses = %lld\n", lru.totalAccess);
    printf("    Total number of page faults = %lld\n", lru.pageFaults);
    printf("    Page fault rate (percentage) = %.2f\n", 100.0 * lru.pageFaults / lru.totalAccess);

    double time_ns = (lru.totalAccess - lru.pageFaults) * MEMACESS_TIME + lru.pageFaults * PAGEFAULT_TIME;
    printf("    Total memory access time = %.2f sec\n\n", time_ns / NS_PER_SEC);
    freeLRU(&lru);
}

static void s5(int k, int f)
{
    LRU lru;
    lru_init(&lru, f);

    long long RGBP = 0;
    long long RGB  = RGBP + SZ_PTR_BLOCK;
    long long rgbp = RGB  + SZ_CHAR3_ARRAY;
    long long rgb  = rgbp + SZ_PTR_BLOCK;

    for (int i = 0; i < ROWS; ++i) {
        int rowstart = (i - k < 0)    ? 0    : i - k;
        int rowend   = (i + k >= ROWS)? ROWS-1: i + k;

        for (int j = 0; j < COLS; ++j) {
            int colstart = (j - k < 0)    ? 0    : j - k;
            int colend   = (j + k >= COLS)? COLS-1: j + k;

            for (int row = rowstart; row <= rowend; ++row) {
                for (int col = colstart; col <= colend; ++col) {
                    int dx = row - i, dy = col - j;
                    if (dx*dx + dy*dy <= k*k) {
                        long long ptrOffset  = (long long)row * PTR_SIZE;
                        long long baseOffset = (long long)row * COLS * 3 + (long long)col * 3;

                        accessPage(&lru, pageOf(RGBP + ptrOffset));
                        accessPage(&lru, pageOf(RGB + baseOffset));
                        accessPage(&lru, pageOf(RGBP + ptrOffset));
                        accessPage(&lru, pageOf(RGB + baseOffset + 1));
                        accessPage(&lru, pageOf(RGBP + ptrOffset));
                        accessPage(&lru, pageOf(RGB + baseOffset + 2));
                    }
                }
            }

            /* Write output pixel */
            long long ptrOffset  = (long long)i * PTR_SIZE;
            long long baseOffset = (long long)i * COLS * 3 + (long long)j * 3;

            accessPage(&lru, pageOf(rgbp + ptrOffset));
            accessPage(&lru, pageOf(rgb + baseOffset));
            accessPage(&lru, pageOf(rgbp + ptrOffset));
            accessPage(&lru, pageOf(rgb + baseOffset + 1));
            accessPage(&lru, pageOf(rgbp + ptrOffset));
            accessPage(&lru, pageOf(rgb + baseOffset + 2));
        }
    }

    printf("+++ Scheme 5: One dynamic array of char\n");
    printf("    Total number of memory accesses = %lld\n", lru.totalAccess);
    printf("    Total number of page faults = %lld\n", lru.pageFaults);
    printf("    Page fault rate (percentage) = %.2f\n", 100.0 * lru.pageFaults / lru.totalAccess);

    double time_ns = (lru.totalAccess - lru.pageFaults) * MEMACESS_TIME + lru.pageFaults * PAGEFAULT_TIME;
    printf("    Total memory access time = %.2f sec\n\n", time_ns / NS_PER_SEC);
    freeLRU(&lru);
}

static void s6(int k, int f)
{
    LRU lru;
    lru_init(&lru, f);

    long long RGBP = 0;
    long long RGB  = RGBP + SZ_PTR_BLOCK;
    long long rgbp = RGB  + SZ_PIXEL_ARRAY;
    long long rgb  = rgbp + SZ_PTR_BLOCK;

    for (int i = 0; i < ROWS; ++i) {
        int rowstart = (i - k < 0)    ? 0    : i - k;
        int rowend   = (i + k >= ROWS)? ROWS-1: i + k;

        for (int j = 0; j < COLS; ++j) {
            int colstart = (j - k < 0)    ? 0    : j - k;
            int colend   = (j + k >= COLS)? COLS-1: j + k;

            for (int row = rowstart; row <= rowend; ++row) {
                for (int col = colstart; col <= colend; ++col) {
                    int dx = row - i, dy = col - j;
                    if (dx*dx + dy*dy <= k*k) {
                        long long ptrOffset = (long long)row * PTR_SIZE;
                        long long dat_off = ((long long)row * COLS + col) * PIXEL_SIZE;

                        accessPage(&lru, pageOf(RGBP + ptrOffset));
                        accessPage(&lru, pageOf(RGB  + dat_off));
                    }
                }
            }

            /* Write output pixel */
            long long ptrOffset = (long long)i * PTR_SIZE;
            long long dat_off = ((long long)i * COLS + j) * PIXEL_SIZE;

            accessPage(&lru, pageOf(rgbp + ptrOffset));
            accessPage(&lru, pageOf(rgb  + dat_off));
        }
    }

    printf("+++ Scheme 6: One dynamic array of struct\n");
    printf("    Total number of memory accesses = %lld\n", lru.totalAccess);
    printf("    Total number of page faults = %lld\n", lru.pageFaults);
    printf("    Page fault rate (percentage) = %.2f\n", 100.0 * lru.pageFaults / lru.totalAccess);

    double time_ns = (lru.totalAccess - lru.pageFaults) * MEMACESS_TIME + lru.pageFaults * PAGEFAULT_TIME;
    printf("    Total memory access time = %.2f sec\n\n", time_ns / NS_PER_SEC);
    freeLRU(&lru);
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <k> <f>\n", argv[0]);
        fprintf(stderr, "  k = blurring radius\n");
        fprintf(stderr, "  f = number of frames (max %d)\n", MAXT_PT_SIZE);
        return 1;
    }
    int k = atoi(argv[1]);
    int f = atoi(argv[2]);

    if (k <= 0) { fprintf(stderr, "k must be positive\n"); return 1; }
    if (f <= 0 || f > MAXT_PT_SIZE) {
        fprintf(stderr, "f must be in [1, %d]\n", MAXT_PT_SIZE);
        return 1;
    }

    printf("+++ k = %d, f = %d\n\n", k, f);
    s1(k, f);
    s2(k, f);
    s3(k, f);
    s4(k, f);
    s5(k, f);
    s6(k, f);
    return 0;
}
