/*
 * Core process system.
 */
#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <array.h>
#include <machine/spl.h>
#include <machine/pcb.h>
#include <process.h>
#include <addrspace.h>
#include <vnode.h>
#include <queue.h>

/* States a process can be in. */
typedef enum {
	S_READY,
	S_WAITING,
	S_RUNNING,
    S_TERMINATED
} process_t;

/*
 * Create a process. This is used both to create the first process's 
 * process structure and to create subsequent processes.
 */

static
struct process *
process_create(static struct process parent)
{
	struct process *process = kmalloc(sizeof(struct process));
	if (process==NULL) {
		return NULL;
	}
	process->p_name = kstrdup(parent->p_name);
	if (process->p_name==NULL) {
		kfree(process);
		return NULL;
	}



	process->p_stack = NULL;
	
	process->p_vmspace = NULL;

	process->p_cwd = NULL;
	
	// If you add things to the thread structure, be sure to initialize
	// them here.
	
	return process;
}

struct process *
process_bootstrap(void){
	struct process *me;

	/* Create the data structures we need. */
	wait_q = q_create(0);
	if (wait_q==NULL) {
		panic("Cannot create waiting queue\n");
	}

	ready_q = q_create(0);
	if (ready_q==NULL) {
		panic("Cannot create ready queue\n");
	}
	
	/*
	 * Create the thread structure for the first thread
	 * (the one that's already running)
	 */
	me = process_create("<boot/menu>");
	if (me==NULL) {
		panic("process_bootstrap: Out of memory\n");
	}

	/*
	 * Leave me->p_stack NULL. This means we're using the boot stack,
	 * which can't be freed.
	 */

	/* Initialize the first process's pcb */
	md_initpcb0(&me->p_pcb);

	/* Set curthread */
	//curthread = me;

	/* Number of threads starts at 1 */
	//numthreads = 1;

	/* Done */
	return me;
}
