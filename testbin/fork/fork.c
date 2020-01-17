/*
 * fork.c
 *
 * simple fork program to help you test fork() and copy-on-write 
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>

static volatile int dummy = 0;

static
int
spin(int n)
{
        int i;
        
        for (i = 0; i < n; ++i) {
	        dummy += i;
	}
	
	return 0;
}

static
int
dofork(void)
{
	int pid;
	pid = fork();
	if (pid < 0) {
		warn("fork");
	}
	return pid;
}

int
main(int argc, const char * argv[])
{
        int i;
	int n = 1;
	
	if (argc == 2) {
	        n = atoi(argv[1]);
	}

	for (i = 0; i < n; ++i) {
		if (dofork() == 0)
		        return spin(500);
	}
	
	/* spin for a while and exit */
	return spin(n*1000);
}
