/*
 * Core process system.
 */
#include <process.h>
#include <array.h>
#include <hashmap.h>
#include <types.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <machine/trapframe.h>
#include <kern/errno.h>
#include <synch.h>
#include <machine/spl.h>


/*************************************************************************************************/
/* Following functions deal with process lookup table, handling PID allocation and reclaimation. */
/*************************************************************************************************/

/* constant for max number of threads */
const int MAX_PID = 32768;

/* Global variable holding process lookup table */
struct array *process_table;

/* Redeclare zombies array used in thread.c */
extern struct array *zombies;

/* Bootstrap - Returns 0 on success, 1 on failure */
void proc_bootstrap() 
{
    process_table = array_create();
    int err = array_preallocate(process_table, MAX_PID);
    if(err) {
        panic("Process bootstrap out of memory!");
    }

    /* Initialize the array to NULL */
    array_setsize(process_table, MAX_PID);
    int index = 0;
    for( index=0; index<MAX_PID; index++) {
        array_setguy(process_table, index, NULL);
    }
}

/* Add an entry - returns the PID, if no more PID's return -1 - thread being passed in should not already be in the table! */
int proc_addentry(void *t_block, pid_t *retval)
{   
    /* Find an available PID */
    /* Index starts at 1 because PID 0 is reserved for swapper or scheduler. PID 1 should be assigned to init, the process created in thread_bootstrap */
    pid_t index;
    for( index=1; index<MAX_PID; index++ ) {
        if(array_getguy(process_table, index) == NULL) {
            array_setguy(process_table, index, t_block);
            *retval = index;
            return 0;
        }
    }
    return EAGAIN;
}

/* Delete an entry - Using setguy because we want the value to be NULL. Later we will switch the hashmaps */
void proc_deleteentry(pid_t pid)
{
    array_setguy(process_table, pid, NULL);
}

/* Destroy process table */
void proc_destroy()
{
    array_destroy(process_table);
}



/********************************************************/
/* Following functions are helpers for the system calls */
/********************************************************/

/* Create a process fork by using thread fork, and jumping to the machine dependant md_forkentry to do a context switch */
int proc_fork(struct trapframe *tf, pid_t *ret_val) 
{
    int err = 0;

    /* copy parent space into new child address space */
    struct addrspace *child_addrspace = NULL;
    err = as_copy(curthread->t_vmspace, &child_addrspace);
    if(err) {
        return err;
    }

    /* 
     * Create trap frame for the child and thread object
     * Note: malloc has to be used here because tf will be removed from the stack after use. We want the child TF to persist
     * even if the parent is terminated and everything about it is destroyed
     */
    struct trapframe *child_tf = (struct trapframe *)kmalloc(sizeof(struct trapframe));
    if(child_tf == NULL) {
        as_destroy(child_addrspace);
        return ENOMEM;
    }

    /* Copy over trap frame */
    *child_tf = *tf;

    /* Find a spot in the process_table */
    pid_t child_pid;
    struct thread *child_thread;
    err = proc_addentry(child_thread, &child_pid);
    if(err) {
        as_destroy(child_addrspace);
        kfree(child_tf);
        return err;
    }

    /* create a fork, child thread is set up to start from md_forkentry */
    err = thread_fork("dont care", (void *)child_tf, (unsigned long)child_addrspace, md_forkentry, &child_thread);
    if(err) {
        as_destroy(child_addrspace);
        kfree(child_tf);
        return err;
    }

    /* update child_thread information */
    child_thread->t_pid = child_pid;
    child_thread->t_ppid = curthread->t_pid;

    /* return */
    *ret_val = child_pid;
    return 0;
}


/* Helper for exit */
void proc_exit(int exitcode) 
{
    /* Disable interrupts, no interruptions for exiting! */
    splhigh();

    curthread->t_exitcode = exitcode;
    curthread->t_exitflag = 1;
    lock_release(curthread->t_exitlock);        // Now others waiting for this pid can continue with their lives

    /* Apparently we need to change the status of this processes children... ADOPTION!! */
    int index;
    for(index=0; index<MAX_PID; index++) {
        struct thread *proc = array_getguy(process_table, index);
        if(proc != NULL) {
            if(curthread->t_pid == proc->t_ppid) {
                proc->t_ppid = 1;
            }
        }
    }

    thread_exit();
}


/* Helper function for sys_wait */
int proc_wait(int pid, int *exitcode)
{
    /* Turn off interrupts */
    int spl;
    spl = splhigh();
    if( pid > MAX_PID || pid < 1 ){
        splx(spl);
        *exitcode = 0;
        return EINVAL;
    }
    
    struct thread *current = array_getguy(process_table, pid);
    if ( current == NULL ){
        splx(spl);
        *exitcode = 0;
        return EINVAL;
    }
    /* Check if waiting only on the pid of the children */
    if( current->t_ppid != curthread->t_pid){
        splx(spl);
        *exitcode = 0;
        return EINVAL;
    }

    if( current->t_exitflag ){
        splx(spl);
        *exitcode = current->t_exitcode;
        return 0;
    }
    /* Wait on the lock until the child terminates its execution */ 
    lock_acquire(curthread->t_exitlock);
    
    /* Call process and thread reaping function */
    *exitcode = current->t_exitcode;
    proc_reap(pid);
    splx(spl);

    return 0;
}

/* reap a process, remove it from process table and zombie array */
int proc_reap(int pid){
    struct thread* to_reap = array_getguy(process_table, pid);
    
    proc_deleteentry(pid);
    lock_destroy(to_reap->t_exitlock);

    int idx;
    for (idx=0; idx<array_getnum(zombies); idx++) {
		struct thread *to_remove = array_getguy(zombies, idx);
        if( to_remove->t_pid == pid ){
            array_remove(zombies, idx);
            thread_destroy(to_reap);
            return 0;
        }
	}

    panic("What the f**k is going on!?");
    return -1;
}
