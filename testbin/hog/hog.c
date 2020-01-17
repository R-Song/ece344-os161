/*
 * hog.c
 * 
 * Spawned by several other user programs to test time-slicing.
 */

int
main(void)
{
	volatile int i;

	for (i=0; i<50000; i++)
		;

	return 0;
}
