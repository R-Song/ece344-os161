/* sbrk.c
 *
 * You can use this program to test sbrk() without dealing with malloc()
 *
 * Kuei Sun <kuei.sun@utoronto.ca>
 *
 * University of Toronto, 2016
 */

#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

#define NUM_INTS 256

int
main(void)
{
    void * brk = sbrk(0);
    int * ptr, i;
    printf("initial break @ %8p\n", brk);
    
    brk = sbrk(NUM_INTS*sizeof(int));
    printf("old break @ %8p\n", brk);
    
    ptr = (int *)brk;
    brk = sbrk(0);
    printf("current break @ %8p\n", brk);
    
    for (i = 0; i < NUM_INTS; ++i)
        ptr[i] = i+1;
    
    for (i = NUM_INTS-1; i >= 0; --i)
        assert(ptr[i] == i+1);
    
    brk = sbrk(1024 * -1024);
    assert(errno == EINVAL);
    
    printf("sbrk() is working\n");   
    return 0;
}

