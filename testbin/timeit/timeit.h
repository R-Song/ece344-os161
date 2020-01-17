#ifndef TIMEIT_H
#define TIMEIT_H

#include <stdlib.h>

struct timespec
{
        time_t        tv_sec;
        unsigned long tv_nsec;
};

void timeit_before(struct timespec * before, struct timespec * after);
void timeit_after(struct timespec * before, struct timespec * after);
void timeit_print(struct timespec * before, struct timespec * after, int k);

time_t timeit_end(time_t start);

int sieve(int nruns);

#endif

