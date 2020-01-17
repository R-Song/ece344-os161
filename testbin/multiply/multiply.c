/*
 * This program multiplies N numbers where N > 1
 *
 * Used to test argument passing to child processes to test the execv() system
 * call implementation. 
 *
 * Test case:
 *      run: p /testbin/multiply 3 5
 *      result expected: 15
 *
 * If option -s is passed, exitcode is the product, and no output is produced.
 *
 * It is intentional that 'multiply -s 5' would return 5 instead of error.
 *
 * Kuei Sun <kuei.sun@utoronto.ca>
 * 
 * University of Toronto, 2013
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mult1 = 0;

int
main(int argc, char *argv[])
{
	int mult2, i = 1, ret = 0;

    if (argc < 3) {
    	const char * progname = "multiply";
    	if ( argc > 0 ) 
    		progname = argv[0];
    	printf("usage: %s [-s] n1 n2 [nN...]\n", progname);
        return -1;
	}
	
	if ( !strcmp(argv[1], "-s") ) {
		i = 2;
		ret = 1;
	}
	
	mult1 = atoi(argv[i++]);
	while ( i < argc ) {
		mult2 = atoi(argv[i++]);
		mult1 *= mult2;
	}

	if ( !ret ) {
		printf("%d\n", mult1);
		return 0;
	}
	
	return mult1;
}

