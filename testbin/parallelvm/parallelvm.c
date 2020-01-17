/*
 * parallelvm.c
 *
 * Highly parallelized VM stress test.
 *
 * This test probably won't run with only 512k of physical memory
 * (unless maybe if you have a *really* gonzo VM system) because each
 * of its processes needs to allocate a kernel stack, and those add up
 * quickly.
 *
 * UPDATE: modified to accept program argument and choose number of processes
 * to run
 *
 * Kuei Sun <kuei.sun@utoronto.ca>
 *
 * University of Toronto, 2017
 *
 */

#include "say.h"
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#define NJOBS    24

#define DIM      35
#define NMATS    11
#define JOBSIZE  ((NMATS+1)*DIM*DIM*sizeof(int))

static const int right_answers[NJOBS] = {
        -1337312809,
	356204544,
	-537881911,
	-65406976,
	1952063315,
	-843894784,
	1597000869,
	-993925120,
	838840559,
        -1616928768,
	-182386335,
	-364554240,
	251084843,
	-61403136,
	295326333,
	1488013312,
	1901440647,
	0,
        -1901440647,
        -1488013312,
	-295326333,
	61403136,
	-251084843,
	364554240,
};

////////////////////////////////////////////////////////////

struct matrix {
	int m_data[DIM][DIM];
};

////////////////////////////////////////////////////////////

static
void
multiply(struct matrix *res, const struct matrix *m1, const struct matrix *m2)
{
	int i, j, k;

	for (i=0; i<DIM; i++) {
		for (j=0; j<DIM; j++) {
			int val=0;
			for (k=0; k<DIM; k++) {
				val += m1->m_data[i][k]*m2->m_data[k][j];
			}
			res->m_data[i][j] = val;
		}
	}
}

static
void
addeq(struct matrix *m1, const struct matrix *m2)
{
	int i, j;
	for (i=0; i<DIM; i++) {
		for (j=0; j<DIM; j++) {
			m1->m_data[i][j] += m2->m_data[i][j];
		}
	}
}

static
int
trace(const struct matrix *m1)
{
	int i, t=0;
	for (i=0; i<DIM; i++) {
		t += m1->m_data[i][i];
	}
	return t;
}

////////////////////////////////////////////////////////////

static struct matrix mats[NMATS];

static
void
populate_initial_matrixes(int mynum)
{
	int i,j;
	struct matrix *m = &mats[0];
	for (i=0; i<DIM; i++) {
		for (j=0; j<DIM; j++) {
			m->m_data[i][j] = mynum+i-2*j;
		}
	}
	
	multiply(&mats[1], &mats[0], &mats[0]);
}

static
void
compute(int n)
{
	struct matrix tmp;
	int i, j;

	for (i=0,j=n-1; i<j; i++,j--) {
		multiply(&tmp, &mats[i], &mats[j]);
		addeq(&mats[n], &tmp);
	}
}

static
void
computeall(int mynum)
{
	int i;
	populate_initial_matrixes(mynum);
	for (i=2; i<NMATS; i++) {
		compute(i);
	}
}

static
int
answer(void)
{
	return trace(&mats[NMATS-1]);
}

static
void
go(int mynum)
{
	int r;

	say("Process %d (pid %d) starting computation...\n", mynum, 
	    (int) getpid());

	computeall(mynum);
	r = answer();

	if (r != right_answers[mynum]) {
		say("Process %d answer %d: FAILED, should be %d\n", 
		    mynum, r, right_answers[mynum]);
		exit(1);
	}
	say("Process %d answer %d: passed\n", mynum, r);
	exit(0);
}

////////////////////////////////////////////////////////////

static
int
status_is_failure(int status)
{
#ifdef HOST
	/* Proper interpretation of Unix exit status */
	if (WIFSIGNALED(status)) {
		return 1;
	}
	if (!WIFEXITED(status)) {
		/* ? */
		return 1;
	}
	status = WEXITSTATUS(status);
#endif
	return status != 0;
}

static
void
makeprocs(int njobs)
{
	int i, status, failcount;
	pid_t pids[NJOBS];

	printf("Job size approximately %lu bytes\n", (unsigned long) JOBSIZE);
	printf("Forking %d jobs; total load %luk\n", njobs,
	       (unsigned long) (njobs * JOBSIZE)/1024);

	for (i=0; i<njobs; i++) {
		pids[i] = fork();
		if (pids[i]<0) {
			warn("fork");
		}
		if (pids[i]==0) {
			/* child */
			go(i);
		}
	}

	failcount=0;
	for (i=0; i<njobs; i++) {
		if (pids[i]<0) {
			failcount++;
		}
		else {
			if (waitpid(pids[i], &status, 0)<0) {
				err(1, "waitpid");
			}
			if (status_is_failure(status)) {
				failcount++;
			}
		}
	}

	if (failcount>0) {
		printf("%d subprocesses failed\n", failcount);
		exit(1);
	}
	printf("Test complete\n");
}

void 
usage(void) {
        printf("usage: parallelvm [NUM=24]\n");
        _exit(-1); 
}

int
main(int argc, const char * argv[])
{
        int num = 0;

        if (argc == 2) {
                num = atoi(argv[1]);
        }
        else if (argc <= 1) {
                num = NJOBS;
        }
        else {
                usage();
        }
        
        if (num <= 0) {
                printf("parallelvm: NUM must be greater than zero\n");
                usage();
        }
        
	makeprocs(num);
	return 0;
}
