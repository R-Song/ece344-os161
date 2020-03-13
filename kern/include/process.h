#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <types.h>

struct trapframe;
struct addrspace;
struct thread;

/*
 * These functions deal with manipulating the process hashtable table. 
 * Remember that the hashtable is just a way of organizing the thread pointers.
 * 
 * These provide the layer of abstraction for the implementation of system calls as in when writing the system calls
 * we don't need to think about operations such as pid allocation and reclaimation, movement from one queue to another, etc...
 * 
 * Actual system call declarations are found in syscall.h, that is where we find fork, exec, getpid, etc...
 */

/* Call this once in thread_bootstrap() to allocate data structures */
void proc_bootstrap();

/* Finds an available pid_t and allocates process entry to the process hashtable */
int proc_addentry(struct thread *thread, pid_t *retval);
int proc_pid_avail();


/* 
 * Reap a process by deallocating it entirely from the process hashtable 
 * The actual thread structure is not freed from memory
 */
void proc_deleteentry(pid_t pid); 

/* Function called in thread_create to initialize process information */
int proc_init(struct thread *child_thread);

/*
 * Following functions are helpers for the system calls
 */
int proc_fork(struct trapframe *tf, pid_t *ret_val);

/*
 * Helper function to exit process
 */
void proc_exit(int exitcode);
/*
 * Implements waitpid(), calls thread_wait; handles reaping of the children process
 */
int proc_wait(int pid, int *status);
/*
 * Reaps the process at given pid; destroys lock and removes it from the zombies array
 */ 
int proc_reap(int pid);
/*
 * Creates new process and makes it execute a different program
 */ 
int proc_execv(char *pathname_k, char **argv, int size_args);

/* destroy process */
void proc_destroy(struct thread *thread);


#endif /* _PROCESS_H_ */
