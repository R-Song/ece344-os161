/*
 * sieve.c
 * 
 * Calculates prime numbers using sieve of Eratosthenes
 *
 * Kuei Sun <kuei.sun@utoronto.ca>
 *
 * University of Toronto, 2016
 *
 */

#include "timeit.h"
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>

#define BITS_PER_WORD   8
#define WORD_TYPE       unsigned char

struct bitmap {
	u_int32_t nbits;
	WORD_TYPE *v;
};

static void
bitmap_init(struct bitmap * b, WORD_TYPE * buf, u_int32_t words)
{
        u_int32_t nbytes = words*sizeof(WORD_TYPE);

	b->v = buf;
	bzero(b->v, nbytes);
	b->nbits = nbytes*BITS_PER_WORD;
}

static inline void
bitmap_translate(u_int32_t bitno, u_int32_t *ix, WORD_TYPE *mask)
{
	u_int32_t offset;
	*ix = bitno / BITS_PER_WORD;
	offset = bitno % BITS_PER_WORD;
	*mask = ((WORD_TYPE)1) << offset;
}

static void
bitmap_mark(struct bitmap *b, u_int32_t index)
{
	u_int32_t ix;
	WORD_TYPE mask;
	assert(index < b->nbits);
	bitmap_translate(index, &ix, &mask);
	b->v[ix] |= mask;
}

static int
bitmap_isset(struct bitmap *b, u_int32_t index)
{
        u_int32_t ix;
        WORD_TYPE mask;
        bitmap_translate(index, &ix, &mask);
        return (b->v[ix] & mask);
}

/* we want to have only 1 page for data region */
#define BUFSIZE 3072
#define NUMBITS BUFSIZE*BITS_PER_WORD

/* run everything on the same buffer */
static struct bitmap b;
static unsigned char buf[BUFSIZE];
 
static void
eratosthenes(int k, int nruns)
{
        struct timespec before, after;
        int i;
        int nl = 0;
        
        timeit_before(&before, &after);
        printf("sieve: calculating prime numbers below %d\n", NUMBITS);
        
        bitmap_init(&b, buf, sizeof(buf));
        
        for (i = 2; i < NUMBITS; ++i) {
                int j = i;

                if (bitmap_isset(&b, i))
                        continue;
                        
                while (j < NUMBITS) {
                        bitmap_mark(&b, j);
                        j += i;
                }
                
                printf("%6d ", i);
                if (++nl == 8) {
                        printf("\n");
                        nl = 0;
                }
        }
        
        if (nl > 0)
                printf("\n");
        
        timeit_after(&before, &after);
        
        if (k < nruns)
                eratosthenes(k+1, nruns);
           
        timeit_print(&before, &after, k);
}

int 
sieve(int nruns)
{
        time_t start, diff;
        
        start = time(NULL);
        eratosthenes(1, nruns);
        diff = timeit_end(start);      
          
        printf("sieve: %d runs took about %d seconds\n", nruns, diff);
        return 0;
}

