#ifndef _PROCESS_H_
#define _PROCESS_H_


/* Get machine-dependent stuff */
#include <types.h>
#include <machine/pcb.h>


/* States a process can be in. */
typedef enum {
	S_READY,
	S_WAITING,
	S_RUNNING,
    S_TERMINATED
} procstate_t;


/*
 * Definition of proc_entry struct. These form the entry in the process lookup table and are essentially process control blocks
 */
// TODO: how does memory work? how does user memory differ from kernel memory. Maybe visit uio.c
// 		 how are hardware states saved?... how does PCB.h work? 
//		 have to save exit code right...
struct addrspace;

struct proc_block {
	// struct pcb p_pcb;
	// char *p_name;
    // pid_t p_ppid;
    // pid_t p_pid;
	// char *p_stack;
	// struct addrspace *p_vmspace;
	// struct vnode *p_cwd;
	int filler;
};


/* 
 * Definition of proc_state struct, used to provide a snapshot of the hardware state of the process.
 * Remember that the kernel has more information about the process than the user does, specifically address space information.
 * It is up to the sys_calls to pass this struct into the appropriate functions
 */
struct proc_state {
	/* Hardware state */
	//TODO: fill this in... 
	int filler;
};


/*
 * These functions deal with manipulating the process hashtable table.
 * These provide the layer of abstraction for the implementation of system calls as in when writing the system calls
 * we don't need to think about operations such as pid allocation and reclaimation, movement from one queue to another, etc...
 * 
 * Actual system call declarations are found in syscall.h, that is where we find fork, exec, getpid, etc...
 */

/* Call this once to allocate data structures */
int proc_bootstrap();

/* Finds an available pid_t and allocates process entry to the process hashtable */
pid_t proc_addentry(struct proc_state proc_info);

/* Updates process entry */
int proc_updateentry(pid_t pid, struct proc_state proc_info);

/* Wake up processes waiting on a pid */
int proc_wakeup_pid(pid_t pid);

/* Reap a process by deallocating it entirely from the process hashtable */
int proc_reap(pid_t pid);


/* 
 * State transition functions:
 * These functions move processes from one state queue to another. Queues are a shared resource so 
 * appropriate sychronization primitives will be neccessary
 */
int proc_transition_ready_running(pid_t pid);			/* Schedule process */
int proc_transition_running_ready(pid_t pid);			/* Unschedule process */
int proc_transition_running_waiting(pid_t pid);			/* I/O, page fault, etc */
int proc_transition_running_terminated(pid_t pid);		/* Process exit */
int proc_transition_waiting_ready(pid_t pid);			/* I/O done, page fault recovered, etc */

#endif /* _PROCESS_H_ */
