/* 
 * stoplight.c
 *
 * You can use any synchronization primitives available to solve
 * the stoplight problem in this file.
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


/*
 * Number of cars created.
 */

#define NCARS 20

/*
 *
 * Function Definitions
 *
 */

static const char *directions[] = { "N", "E", "S", "W" };

static const char *msgs[] = {
        "approaching:",
        "region1:    ",
        "region2:    ",
        "region3:    ",
        "leaving:    "
};

/* use these constants for the first parameter of message */
enum { APPROACHING, REGION1, REGION2, REGION3, LEAVING };

/* use these constants for directions */
enum { N, E, S, W };

/* use these constants for turns */
enum { STRAIGHT, RIGHT, LEFT };

static void
message(int msg_nr, int carnumber, int cardirection, int destdirection)
{
    kprintf("%s car = %2d, direction = %s, destination = %s\n",
            msgs[msg_nr], carnumber,
            directions[cardirection], directions[destdirection]);
}

/* 
 * Locks and CV
 */

/* Lock for printing messages to the screen */
struct lock *msg_lock;
/* Lock for changing acquire and release of other locks. This prevents deadlocks... */
struct lock *mod_lock;
/* Locks for each of the quadrants */
struct lock *nw_lock;
struct lock *ne_lock;
struct lock *sw_lock;
struct lock *se_lock;


/*
 * gostraight()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement passing straight through the
 *      intersection from any direction.
 *      Write and comment this function.
 */

static void
gostraight(unsigned long cardirection, unsigned long carnumber)
{
	lock_acquire(msg_lock);

	int destdirection;
	switch(cardirection) {
		case N:
			destdirection = S;
			break;
		case E:
			destdirection = W;
			break;
		case S:
			destdirection = N;
			break;
		case W:
			destdirection = E;
			break;
		default:
			/* if reaches this point, something is wrong */
			assert(cardirection < 3 );
	}
	
	message(APPROACHING, carnumber, cardirection, destdirection);
	message(REGION1, carnumber, cardirection, destdirection);
	message(REGION2, carnumber, cardirection, destdirection);
	message(LEAVING, carnumber, cardirection, destdirection);

	lock_release(msg_lock);
}


/*
 * turnleft()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a left turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
turnleft(unsigned long cardirection, unsigned long carnumber)
{
	lock_acquire(msg_lock);

	int destdirection;
	switch(cardirection) {
		case N:
			destdirection = E;
			break;
		case E:
			destdirection = S;
			break;
		case S:
			destdirection = W;
			break;
		case W:
			destdirection = N;
			break;
		default:
			/* if reaches this point, something is wrong */
			assert(cardirection < 3 );
	}
	
	message(APPROACHING, carnumber, cardirection, destdirection);
	message(REGION1, carnumber, cardirection, destdirection);
	message(REGION2, carnumber, cardirection, destdirection);
	message(REGION3, carnumber, cardirection, destdirection);
	message(LEAVING, carnumber, cardirection, destdirection);

	lock_release(msg_lock);
}


/*
 * turnright()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a right turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
turnright(unsigned long cardirection, unsigned long carnumber)
{
	lock_acquire(msg_lock);

	int destdirection;
	switch(cardirection) {
		case N:
			destdirection = W;
			break;
		case E:
			destdirection = S;
			break;
		case S:
			destdirection = E;
			break;
		case W:
			destdirection = N;
			break;
		default:
			/* if reaches this point, something is wrong */
			assert(cardirection < 3 );
	}
	
	message(APPROACHING, carnumber, cardirection, destdirection);
	message(REGION1, carnumber, cardirection, destdirection);
	message(LEAVING, carnumber, cardirection, destdirection);

	lock_release(msg_lock);
}

/* 
 * Helper function to approachintersection(). Given a car direction and car turn, 
 * return 1 if the regions locks are available, return 0 if any of them are unavailable
 */
int is_road_available(int cardirection, int carturn)
{
	(void) cardirection;
	(void) carturn;
	// use lock_do_i_hold(...) function
	return 0;
}

/*
 * approachintersection()
 *
 * Arguments: 
 *      void * unusedpointer: currently unused.
 *      unsigned long carnumber: holds car id number.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Change this function as necessary to implement your solution. These
 *      threads are created by createcars().  Each one must choose a direction
 *      randomly, approach the intersection, choose a turn randomly, and then
 *      complete that turn.  The code to choose a direction randomly is
 *      provided, the rest is left to you to implement.  Making a turn
 *      or going straight should be done by calling one of the functions
 *      above.
 */
 
static
void
approachintersection(void * unusedpointer, unsigned long carnumber)
{
	(void) unusedpointer;
	(void) gostraight;
	(void) turnleft;
	(void) turnright;
	(void) is_road_available;

	/* 
	 * Assigns cardirection and carturn randomly
	 * refer to the enums for what the integers mean 
	 */
	int cardirection = random() % 4;
	int carturn = random() % 3;
	int turn_complete = 0;

	(void) cardirection;
	(void) carturn;
	(void) carnumber;

	while(turn_complete == 0) 
	{
		lock_acquire(mod_lock);
		/* Determine which quadrants need to be attained to perform turn */


		
		

		

		/* Test to see if the appropriate regions has released locks */



		/* Acquire the locks and complete turn */


		/* Loop back and try again */

		lock_release(mod_lock);
	}
}


/*
 * createcars()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up the approachintersection() threads.  You are
 *      free to modiy this code as necessary for your solution.
 */

int
createcars(int nargs, char ** args)
{	
	/* Create synchronization primitives */
	(struct lock *)msg_lock = (struct lock *)lock_create("msg_lock");
	(struct lock *)mod_lock = (struct lock *)lock_create("mod_lock");
	(struct lock *)nw_lock = (struct lock *)lock_create("nw_lock");
	(struct lock *)ne_lock = (struct lock *)lock_create("ne_lock");
	(struct lock *)sw_lock = (struct lock *)lock_create("sw_lock");
	(struct lock *)se_lock = (struct lock *)lock_create("se_lock");

	int index, error;

	/*
	* Start NCARS approachintersection() threads.
	*/
	for (index = 0; index < NCARS; index++) {
		error = thread_fork("approachintersection thread",
							NULL, index, approachintersection, NULL);
		/*
		* panic() on error.
		*/
		if (error) {         
			panic("approachintersection: thread_fork failed: %s\n",
					strerror(error));
		}
	}
	
	/* wait until all other threads finish */
	while (thread_count() > 1)
		thread_yield();

	/* destroy locks */


	(void)message;
	(void)nargs;
	(void)args;
	kprintf("stoplight test done\n");

	return 0;
}

