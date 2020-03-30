#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <types.h>

struct trapframe;
struct addrspace;
struct thread;

/********************************************************************************************/
/* Functions that deal with process lookup table, handling PID allocation and reclaimation, */
/*              process initialization, destruction, and reaping                            */
/********************************************************************************************/

/* Call this once in thread_bootstrap() to create process table */
void    proc_bootstrap();

/* Finds available pid_t and updates process table */
int     proc_addentry(struct thread *thread, pid_t *retval);

/* Returns 1 if a pid is available, 0 if not */
int     proc_pid_avail();

/* Delete process from process table */
void    proc_deleteentry(pid_t pid); 

/* Initialize process information */
int     proc_init(struct thread *child_thread);

/* Destroy a process */
void    proc_destroy(struct thread *thread);

/* Reaps the process with given pid, called by the parent */ 
void    proc_reap(pid_t pid);

/* Deallocate process table */
void    proc_shutdown();

/* Stat function for debugging */
void    proc_stat();


/********************************************************/
/* Following functions are helpers for the system calls */
/********************************************************/

/* Helper function for sys_fork() */
int     proc_fork(struct trapframe *tf, pid_t *ret_val);

/* Helper function for sys_exit() */
void    proc_exit(int exitcode);

/* Helper function for sys_waitpid() */
int     proc_waitpid(pid_t pid, int *status);

/* Helper function for sys_execv() */ 
int     proc_execv(char *program, int argc, char **argv);


#endif /* _PROCESS_H_ */

