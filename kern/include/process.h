#ifndef _PROCESS_H_
#define _PROCESS_H_

/*
 * Definition of a process.
 */

/* Get machine-dependent stuff */
#include <machine/pcb.h>

static const unsigned int MAX_PID = 32768

struct addrspace;

struct process {
	/**********************************************************/
	/* Private process members - internal to the process system */
	/**********************************************************/
	
	struct pcb p_pcb;
	char *p_name;
    pid_t p_ppid;
    pid_t p_pid;
	char *p_stack;
	
	/**********************************************************/
	/* Public process members - can be used by other code      */
	/**********************************************************/
	
	/*
	 * This is public because it isn't part of the process system,
	 * and will need to be manipulated by the userprog and/or vm
	 * code.
	 */
	struct addrspace *p_vmspace;

	/*
	 * This is public because it isn't part of the process system,
	 * and is manipulated by the virtual filesystem (VFS) code.
	 */
	struct vnode *p_cwd;
};