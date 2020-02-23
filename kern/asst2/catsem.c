/*
 * catsem.c
 *
 * Please use SEMAPHORES to solve the cat syncronization problem in 
 * this file.
 */


/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>
#include "catmouse.h"

/*
 * 
 * Function Definitions
 * 
 */

/* States of bowls */
#define NONE 0
#define CAT 1
#define MOUSE 2 
/* Keep track of bowls */
int bowl[NFOODBOWLS];
/* Keep a watch on who modifies the bowl statuses */
struct semaphore *status_sem;
/* Use this semaphore to keep track of when all threads are done */
struct sempahore *thread_sem;


/*
 * catsem()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds the cat identifier from 0 to NCATS - 1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using semaphores.
 *
 */

static void
catsem(void *unusedpointer, unsigned long catnumber)
{	
	int iteration_num = 0;
	int i;
	(void) unusedpointer;

	while(iteration_num < NMEALS) {
		P(status_sem);
		/* Check if there are any mice, if yes do nothing */
		for(i=0; i<NFOODBOWLS; i++) {
			if(bowl[i] == MOUSE) {
				goto finished;
			}
		}
		/* Check if any bowls are open */
		for(i=0; i<NFOODBOWLS; i++) {
			if(bowl[i] == NONE) {
				bowl[i] = CAT;
				goto eat;
			}
		}
		goto finished; /* No bowls available to eat... */

		eat:
			V(status_sem);
			catmouse_eat("cat", catnumber, i+1, iteration_num);
			P(status_sem);
			bowl[i] = NONE;
			iteration_num++;

		finished:
			V(status_sem);
	}
	/* This thread is done, tell the main function this */
	V((struct semaphore *)thread_sem);
}
      

/*
 * mousesem()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long mousenumber: holds the mouse identifier from 0 to 
 *              NMICE - 1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using semaphores.
 *
 */

static void
mousesem(void *unusedpointer, unsigned long mousenumber)
{
	int iteration_num = 0;
	int i;
	(void) unusedpointer;

	while(iteration_num < NMEALS) {
		P(status_sem);
		/* Check if there are any cats, if yes do nothing */
		for(i=0; i<NFOODBOWLS; i++) {
			if(bowl[i] == CAT) {
				goto finished;
			}
		}
		/* Check if any bowls are open */
		for(i=0; i<NFOODBOWLS; i++) {
			if(bowl[i] == NONE) {
				bowl[i] = MOUSE;
				goto eat;
			}
		}
		goto finished; /* No bowls available to eat... */

		eat:
			V(status_sem);
			catmouse_eat("mouse", mousenumber, i+1, iteration_num);
			P(status_sem);
			bowl[i] = NONE;
			iteration_num++;

		finished:
			V(status_sem);
	}
	/* This thread is done, tell the main function this */
	V((struct semaphore *)thread_sem);
}


/*
 * catmousesem()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up catsem() and mousesem() threads.  Change this 
 *      code as necessary for your solution.
 */

int
catmousesem(int nargs, char ** args)
{
	int index, error;

	/* Describes the state of the bowls */
	int i;
	for(i=0; i<NFOODBOWLS; i++) {
		bowl[i] = NONE;
	}
	/* Makes sure only one thread makes a decision at any one time */
	(struct semaphore *)thread_sem = (struct semaphore *)sem_create("ThreadSem", 0);
	(struct semaphore *)status_sem = (struct semaphore *)sem_create("StatusSem", 1); 

	/* Start NCATS catsem() threads */
	for (index = 0; index < NCATS; index++) {
		error = thread_fork("catsem Thread", NULL, index, catsem, NULL); 
		/* panic() on error */
		if (error) {    
			panic("catsem: thread_fork failed: %s\n", 
				strerror(error)
			);
		}
	}
	
	/* Start NMICE mousesem() threads */
	for (index = 0; index < NMICE; index++) {
		error = thread_fork("mousesem Thread", NULL, index, mousesem, NULL);
		/* panic() on error */
		if (error) {
			panic("mousesem: thread_fork failed: %s\n", 
				strerror(error)
			);
		}
	}

	/* wait until all other threads finish */
	while (thread_count() > 1)
			thread_yield();

	(void)nargs;
	(void)args;
	kprintf("catsem test done\n");

	/* Stall until all threads are done running */
	for(i=0; i<(NMICE + NCATS); i++) {
		P((struct semaphore *)thread_sem);
	}

	/* Destroy semaphores */
	sem_destroy((struct semaphore *)thread_sem);
	sem_destroy((struct semaphore *)status_sem);


	return 0;
}

