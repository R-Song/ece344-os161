/*
 * Core process system.
 */
#include <process.h>
#include <types.h>
#include <queue.h>
#include <hashmap.h>
#include <array.h>
#include <addrspace.h>
#include <uio.h>
#include <lib.h>
#include <synch.h>

/* Max number of process */
static const unsigned int MAX_PID = 32768;

/* State queues, currently only need a ready queue but can add more in the future */

/* Wait_pid hashmap, the pid that a process is waiting on is the hash */

/* synchronization primitives */

