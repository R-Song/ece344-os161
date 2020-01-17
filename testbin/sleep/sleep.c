/*
 * sleep.c
 *
 * tests if sleep works (using __time)
 *
 * Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
 *
 * University of Toronto
 *
 * 2016
 */

#include <unistd.h>
#include <stdio.h>

#define SLEEP_SECS 2

int
main(void)
{
        time_t start;
        time_t end;
        int diff;
        
        time(&start);
        sleep(SLEEP_SECS);
        time(&end);
        
        diff = end - start;
        
        /* we account for the inaccuracy of the clock in the latter case */
        if (diff > SLEEP_SECS || (diff+1) < SLEEP_SECS)
            printf("sleep: test failed. slept for %d second(s)\n", diff);
        else
            printf("sleep: test completed.\n");
        
        return 0;
}
