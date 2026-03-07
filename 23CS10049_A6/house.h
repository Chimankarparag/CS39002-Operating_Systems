#ifndef HOUSE_H
#define HOUSE_H

#include <signal.h>
#include <time.h>

#define EMPTY 0
#define DEMON_INSIDE 1
#define NOMAD_INSIDE 2

// Shared memory structure H
typedef struct {
    int state;
    int demon_count;
    int nomad_count;
    int demon_wait_count;
    int nomad_wait_count;
} house_t;

// Tokens for IPC generation
#define TOK_MTX 101
#define TOK_CND 102
#define TOK_SHM 103

#endif