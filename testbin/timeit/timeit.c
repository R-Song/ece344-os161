/*
 * timeit.c
 * 
 * Checks how much time it takes to run a specified program NUM times
 *
 * Kuei Sun <kuei.sun@utoronto.ca>
 *
 * University of Toronto, 2016
 *
 */

#include "timeit.h"
#include <err.h>
#include <stdio.h>
#include <unistd.h>

static int 
runprogram(char * args[])
{
        pid_t pid;

        if ((pid = fork()) == 0) {
                execv(args[0], args);
                warn("execv");
                _exit(-1);
        }
        else if (pid > 0) {
                int x; 
                if (waitpid(pid, &x, 0) < 0) {
                        warn("waitpid");
                        return -1;
                }

                /* don't care */
                (void)x;
        }
        else
        {
                warn("fork");
                return -1;
        }
        
        return 0;
}

static int
timespec_lte(struct timespec * lhs, struct timespec * rhs)
{
        if ((lhs->tv_sec   < rhs->tv_sec) ||
           ((lhs->tv_sec  == rhs->tv_sec) && 
            (lhs->tv_nsec <= rhs->tv_nsec))) 
        {
            return 1;
        }
        
        return 0;
}

void 
timeit_before(struct timespec * before, struct timespec * after)
{
        time_t ret = __time(&before->tv_sec, &before->tv_nsec);
        
        if (ret != before->tv_sec) {
                printf("timeit: failed. inconsistent seconds returned\n");
                exit(-1);
        }        
        
        (void)after;
}

void 
timeit_after(struct timespec * before, struct timespec * after)
{
        after->tv_sec = __time(NULL, &after->tv_nsec);
        
        if (timespec_lte(after, before)) {
                printf("timeit: failed. time after <= time before\n");
                exit(-1);
        }
}

void 
timeit_print(struct timespec * rhs, struct timespec * lhs, int k)
{
        struct timespec ts;
        
        if (lhs->tv_nsec < rhs->tv_nsec) {
                ts.tv_nsec = 1000000000 + lhs->tv_nsec - rhs->tv_nsec;
                ts.tv_sec = lhs->tv_sec - rhs->tv_sec - 1;
        }
        else {
                ts.tv_nsec = lhs->tv_nsec - rhs->tv_nsec;
                ts.tv_sec = lhs->tv_sec - rhs->tv_sec;
        }
        
        printf("run %d took %d.%09lu seconds\n", k, ts.tv_sec, ts.tv_nsec);
}

static int 
go(int i, int nruns, char * args[])
{
        struct timespec before, after;
        int success;
        
        timeit_before(&before, &after);
        
        success = runprogram(args);
        if (success < 0)
                return success;
        
        timeit_after(&before, &after);

        if (i < nruns)
                go(i+1, nruns, args);
        
        timeit_print(&before, &after, i);
        return 0;
}

static int
usage(void)
{
        printf("usage: timeit [NUM PROG [ARGS...]]\n");
        _exit(-1);
}

time_t
timeit_end(time_t start)
{
        time_t end;
        
        time(&end);

        if (start > end) {
                printf("timeit: failed. start time %d > end time %d\n", 
                       start, end);
                exit(-1);
        }
        
        return end - start;
}

static int
timeit(const char * argv[])
{
        time_t start, diff;
        int nruns = 0;
        
        nruns = atoi(argv[1]);
        if (nruns <= 0) {
                printf("timeit: NUM must be greater than zero\n");
                usage();
        }
        
        start = time(NULL);
        
        if (go(1, nruns, (char **)(argv+2)) < 0) {
                printf("timeit: failed. cannot run program %d times\n", nruns);
                return -1;
        }
        
        diff = timeit_end(start);     
        printf("timeit: %d runs took about %d seconds\n", nruns, diff);
        return 0;
}

int
main(int argc, const char * argv[])
{
        if (argc <= 1) {
                return sieve(2);
        }
        else if (argc < 3) {
                usage();
        }
        
        return timeit(argv);
}

