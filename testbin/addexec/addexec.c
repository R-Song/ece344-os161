/*
 * addexec.c
 *
 * Simple program to add two numbers (given in as arguments). Used to
 * test argument passing to child processes via execv().
 *
 * Intended for the basic system calls assignment; this should work
 * once execv() argument handling is implemented, but command line
 * arguments does not need to work.
 *
 * Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
 *
 * University of Toronto
 *
 * 2016
 */

#include <unistd.h>
#include <err.h>

int
main(void)
{
    char prog[] = "testbin/add";
    char * args[4];
    
	args[0] = prog;
	args[1] = (char *)"7";
	args[2] = (char *)"1234560";
	args[3] = NULL;
	execv(prog, args);
	
	/* execv failed */
	warnx("execv");
	return 0;
}
