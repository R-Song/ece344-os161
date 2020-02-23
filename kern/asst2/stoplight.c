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
#include <queue.h>
#include <machine/spl.h>


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
 * Locks
 */

/* Lock for modifying quadrant_lock_arr */
struct lock *mod_lock;
/* Locks for each of the quadrants, acquire before turning */
struct lock *quadrant_lock_arr[2][2];
/* Lock for changing the state of traffic queues */
struct lock *queue_lock_arr[4];

/*
 * Traffic queues
 */
struct queue *cardir_queue_arr[4];


/*
 * get_dest()
 * Returns the destination direction based off of car direction and car turn
 */

static int
get_dest(unsigned long cardirection, unsigned long carturn)
{
	unsigned long destdirection;
	switch(carturn) 
	{
		case STRAIGHT:
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
					assert(cardirection < 4 ); /* if reaches this point, something is wrong */
			}
			break;

		case LEFT:
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
					assert(cardirection < 4 );
			}
			break;

		case RIGHT:
			switch(cardirection) {
				case N:
					destdirection = W;
					break;
				case E:
					destdirection = N;
					break;
				case S:
					destdirection = E;
					break;
				case W:
					destdirection = S;
					break;
				default:
					assert(cardirection < 4 );
			}
			break;

		default:
			assert(carturn < 3);
	}
	return destdirection;
}

/*
 * get_quadrants()
 * Pass in 2x2 array representing the quadrants of the intersection, intialized all at 0;
 * Depending on the cardirection and destdirection, the neccessary quadrants will be assigned value 1.
 */
static void
get_quadrants(int quadrants[2][2], unsigned long cardirection, unsigned long carturn)
{
	switch(cardirection) {
		case N:
			switch(carturn) {
				case STRAIGHT:
					quadrants[0][0] = 1;
					quadrants[1][0] = 1;
					break;
				case RIGHT:
					quadrants[0][0] = 1;
					break;
				case LEFT:
					quadrants[0][0] = 1;
					quadrants[1][0] = 1;
					quadrants[1][1] = 1;
					break;
				default:
					assert(carturn < 3); // should not ever reach here
			}		
			break;

		case E:
			switch(carturn) {
				case STRAIGHT:
					quadrants[0][1] = 1;
					quadrants[0][0] = 1;
					break;
				case RIGHT:
					quadrants[0][1] = 1;
					break;
				case LEFT:
					quadrants[0][1] = 1;
					quadrants[0][0] = 1;
					quadrants[1][0] = 1;
					break;
				default:
					assert(carturn < 3);
			}		
			break;

		case S:
			switch(carturn) {
				case STRAIGHT:
					quadrants[1][1] = 1;
					quadrants[0][1] = 1;
					break;
				case RIGHT:
					quadrants[1][1] = 1;
					break;
				case LEFT:
					quadrants[1][1] = 1;
					quadrants[0][1] = 1;
					quadrants[0][0] = 1;
					break;
				default:
					assert(carturn < 3);
			}
			break;

		case W:
			switch(carturn) {
				case STRAIGHT:
					quadrants[1][0] = 1;
					quadrants[1][1] = 1;
					break;
				case RIGHT:
					quadrants[1][0] = 1;
					break;
				case LEFT:
					quadrants[1][0] = 1;
					quadrants[1][1] = 1;
					quadrants[0][1] = 1;
					break;
				default:
					assert(carturn < 3);
			}
			break;

		default:
			assert(cardirection < 4); // should never reach here
	}
}

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
	int spl;
	int destdirection = get_dest(cardirection, STRAIGHT);

	spl = splhigh();
	message(REGION1, carnumber, cardirection, destdirection);
	splx(spl);

	spl = splhigh();
	message(REGION2, carnumber, cardirection, destdirection);
	splx(spl);

	spl = splhigh();
	message(LEAVING, carnumber, cardirection, destdirection);
	splx(spl);
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
	int spl;
	int destdirection = get_dest(cardirection, LEFT);

	spl = splhigh();
	message(REGION1, carnumber, cardirection, destdirection);
	splx(spl);

	spl = splhigh();
	message(REGION2, carnumber, cardirection, destdirection);
	splx(spl);

	spl = splhigh();
	message(REGION3, carnumber, cardirection, destdirection);
	splx(spl);

	spl = splhigh();
	message(LEAVING, carnumber, cardirection, destdirection);
	splx(spl);
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
	int spl;
	int destdirection = get_dest(cardirection, RIGHT);

	spl = splhigh();
	message(REGION1, carnumber, cardirection, destdirection);
	splx(spl);

	spl = splhigh();
	message(LEAVING, carnumber, cardirection, destdirection);
	splx(spl);
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

	/* 
	 * Assigns cardirection and carturn randomly
	 * refer to the enums for what the integers mean 
	 */
	unsigned int cardirection = random() % 4;
	unsigned int carturn = random() % 3;

	/* 
	 * 2d array to represent the quadrants. 
	 * Ex. quadrants[0][0] represents nw and quadrants[1][0] represents sw
	 * Value of 1 means that quadrant is needed for the proposed turn, 0 otherwise 
	 */
	int quadrants[2][2] = {{0},{0}};
	get_quadrants(quadrants, cardirection, carturn);

	/* Acquire the appropriate lock for the queue */
	lock_acquire(queue_lock_arr[cardirection]);
	q_addtail(cardir_queue_arr[cardirection], &carnumber);
	lock_release(queue_lock_arr[cardirection]);

	/* Spin in a while loop until we are first in the queue */
	while(1)
	{
		lock_acquire(queue_lock_arr[cardirection]);
		int *q_head = q_getguy( cardir_queue_arr[cardirection], q_getstart(cardir_queue_arr[cardirection]) );
		if(*q_head == (int)carnumber) {
			lock_release(queue_lock_arr[cardirection]);
			break;
		}
		lock_release(queue_lock_arr[cardirection]);
	}

	/* first in queue, approach the intersection */
	int destdirection = get_dest(cardirection, carturn);

	int spl = splhigh();
	message(APPROACHING, carnumber, cardirection, destdirection);
	splx(spl);

	/* iterators */
	int i, j;
	/* loop until all neccessary locks are acquired and turns are made */
	while(1) 
	{
		/* Acquire mod_lock before checking for locks and acquiring them... */
		lock_acquire(mod_lock);
		/* Check if all neccessary locks are released, if any are held, jump to red_light label */
		for(i=0; i<2; i++) {
			for(j=0; j<2; j++) {
				if(quadrants[i][j] == 1 && quadrant_lock_arr[i][j]->held == 1) {
					goto red_light;
				}
			}
		}
		/* Acquire all neccessary locks */
		for(i=0; i<2; i++) {
			for(j=0; j<2; j++) {
				if(quadrants[i][j] == 1 && quadrant_lock_arr[i][j]->held == 0) {
					lock_acquire(quadrant_lock_arr[i][j]);
				}
			}
		}
		lock_release(mod_lock); // don't need this anymore

		/* Perform turn */
		if(carturn == STRAIGHT)
			gostraight(cardirection, carnumber);
		else if(carturn == RIGHT)
			turnright(cardirection, carnumber);
		else if(carturn == LEFT)
			turnleft(cardirection, carnumber);
		
		/* release locks, reacquire mod_lock to do so */
		lock_acquire(mod_lock);
		for(i=0; i<2; i++) {
			for(j=0; j<2; j++) {
				if(quadrants[i][j] == 1 && lock_do_i_hold(quadrant_lock_arr[i][j])) {
					lock_release(quadrant_lock_arr[i][j]);
				}
			}
		}
		lock_release(mod_lock);

		/* pop ourselves off the traffic queue */
		lock_acquire(queue_lock_arr[cardirection]);
		q_remhead(cardir_queue_arr[cardirection]);
		lock_release(queue_lock_arr[cardirection]);

		/* turncomplete, break */
		break;

		red_light:
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
	int i, j;
	/* Create synchronization primitives */
	(struct lock *)mod_lock = (struct lock *)lock_create("mod_lock");
	for(i=0; i<2; i++) {
		for(j=0; j<2; j++) {
			(struct lock *)quadrant_lock_arr[i][j] = (struct lock *)lock_create("quadrant_lock");
		}
	}
	for(i=0; i<4; i++) {
		(struct lock *)queue_lock_arr[i] = (struct lock *)lock_create("queue_lock");
	}

	/* Create traffic queues */
	for(i=0; i<4; i++) {
		(struct queue *)cardir_queue_arr[i] = (struct queue *)q_create(NCARS);
	}

	int index, error;

	/* Start NCARS approachintersection() threads */
	for (index = 0; index < NCARS; index++) {
		error = thread_fork("approachintersection thread",
							NULL, index, approachintersection, NULL);
		/* panic() on error */
		if (error) {         
			panic("approachintersection: thread_fork failed: %s\n",
				  strerror(error));
		}
	}
	
	/* wait until all other threads finish */
	while (thread_count() > 1)
		thread_yield();

	/* destroy locks */
	for(i=0; i<2; i++) {
		for(j=0; j<2; j++)
			lock_destroy(quadrant_lock_arr[i][j]);
	}
	for(i=0; i<4; i++) {
		lock_destroy(queue_lock_arr[i]);
	}
	lock_destroy(mod_lock);
	
	/* destroy queues */
	for(i=0; i<4; i++)
		q_destroy(cardir_queue_arr[i]);

	/* surpress unused variable warnings */
	(void)nargs;
	(void)args;

	/* Done */
	kprintf("stoplight test done\n");
	return 0;
}

