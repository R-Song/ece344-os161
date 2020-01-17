/*
 * facts.c
 *
 * calculates factorial sum up to n=12 (n > 12 will cause 32-bit int overflow)
 *
 * this program can test execv() with multiple fork(), and specifically 
 * requires that the exitcode be passed back correctly.
 *
 * If in doubt, see http://mathworld.wolfram.com/FactorialSums.html and
 *                  http://oeis.org/A007489
 *
 * Kuei Sun <kuei.sun@utoronto.ca>
 *
 * University of Toronto, 2013
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>

static char * args[] = {
	(char *)"multiply",
	(char *)"-s",
	(char *)"1",
	(char *)"2",
	(char *)"3",
	(char *)"4",
	(char *)"5",
	(char *)"6",
	(char *)"7",
	(char *)"8",
	(char *)"9",
	(char *)"10",
	(char *)"11",
	(char *)"12",
	NULL,
};

#define NUM_ARGS (int)(sizeof(args)/sizeof(char *))
#define MAX_PRODUCTS ((NUM_ARGS) - 3)
static int pids[MAX_PRODUCTS] = {0};
static int ret[MAX_PRODUCTS] = {0};

static void dofork(int n)
{
	int p;
	while ( n > 0 ) {
		n--;
		p = fork();
		if ( p < 0 ) {
			err(-1, "%d: fork failed", n);
		} 
		else if ( p > 0 ) {
			printf("%d! ", n+1);
			if ( n )
				putchar('+');
			else
				putchar('=');
			pids[n] = p;
		}
		else /* p == 0 (child) */ {
			args[n+3] = NULL;
			execv("testbin/multiply", args);
			err(-1, "%d: execv failed", n);
		}
	}
}

static void dowait(int n)
{
	int i;
	for ( i=0; i < n; i++ ) {
		if ( waitpid(pids[i], ret + i, 0) < 0 ) {
			warn("waitpid for %d", pids[i]);
		}
	}
}

static int sum(int n)
{
	int i, s = 0;
	for ( i=0; i < n; i++ ) {
		s += ret[i];
	}
	return s;
}

int
main(int argc, char *argv[])
{
	int n = MAX_PRODUCTS;

	if ( argc == 2 ) {
		n = atoi(argv[1]);
		if ( n < 1 || n > MAX_PRODUCTS ) {
			printf("usage: %s [n]\n"
			       "       n: from 1 to %d\n", argv[0], MAX_PRODUCTS);
			return -1;
		}
	}
	
	dofork(n);
	dowait(n);
	printf(" %d\n", sum(n));
	
	return 0;
}
