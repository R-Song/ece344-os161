/*
 * exittest.c
 *
 * Test implementation of the _exit syscall. Just calls _exit().
 *
 * Ok, so this one is kind of silly.
 *
 * -------------------------------------------------------------
 *
 * Originated from CSC369 assignments. 
 *
 * Modified by Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
 *
 * University of Toronto
 *
 * 2016
 */
 
#include <unistd.h>
#include <stdio.h>

int
main(void)
{
	volatile int i;

    /*
     * Waste some cpu times before we actually exit -- in case menu does
     * not wait for the program to complete
     */
	for (i=0; i<50000; i++)
		;

	_exit(42);
    
    /*
     * should never get here
     */
    printf("exittest failed.\n");
    reboot(RB_POWEROFF);
    
    /* if you can get here -- you tried too hard to subvert the tester */
    return 0;
}
