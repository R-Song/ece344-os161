/*
 * Core process system.
 */
#include <process.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/limits.h>
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
#include <elf.h>
#include <vfs.h>
/*************************************************************************************************/
/* Following functions deal with process lookup table, handling PID allocation and reclaimation. */
/*************************************************************************************************/

/* constant for max number of threads */
const int MAX_PID = 150;

/* Global variable holding process lookup table */
struct thread **process_table;

/* Redeclare zombies array used in thread.c */
extern struct array *zombies;

/* Bootstrap - Returns 0 on success, 1 on failure */
void proc_bootstrap() 
{
    (struct thread **)process_table = (struct thread **)kmalloc(MAX_PID*sizeof(struct thread *));
    if(process_table == NULL) {
        panic("Process bootstrap out of memory!");
    }

    /* Initialize the array to NULL */
    int index = 0;
    for( index=0; index<MAX_PID; index++) {
        process_table[index] = NULL;
    }
}

/* Add an entry - returns the PID, if no more PID's return -1 - thread being passed in should not already be in the table! */
int proc_addentry(struct thread *thread, pid_t *retval)
{   
    /* Find an available PID */
    /* Index starts at 1 because PID 0 is reserved for swapper or scheduler. PID 1 should be assigned to init, the process created in thread_bootstrap */
    pid_t index;
    for( index=1; index<MAX_PID; index++ ) {
        if(process_table[index] == NULL) {
            process_table[index] = thread; 
            *retval = index;
            return 0;
        }
    }
    return EAGAIN;
}

/* Returns 1 if there is an available PID, 0 otherwise */
int proc_pid_avail()
{   
    /* Find an available PID */
    /* Index starts at 1 because PID 0 is reserved for swapper or scheduler. PID 1 should be assigned to init. */
    pid_t index;
    for( index=1; index<MAX_PID; index++ ) {
        if(process_table[index] == NULL) {
            return 1;
        }
    }
    return 0;
}

/* Delete an entry - Using setguy because we want the value to be NULL. Later we will switch the hashmaps */
void proc_deleteentry(pid_t pid)
{
    process_table[pid] = NULL;
}

/* Destroy process table */
void proc_table_destroy()
{
    kfree(process_table);
}

/* Initializes the process informaton, this function is called in thread_create() */
int proc_init(struct thread *child_thread) {

    int spl = splhigh();

    /* pid allocation */
    pid_t child_pid;
    int err = proc_addentry(child_thread, &child_pid);
    if(err) {
        splx(spl);
        return err;
    }

    /* update all the fields */
    child_thread->t_pid = child_pid;
    if(child_pid == 1) {
        child_thread->t_ppid = 0;
    } 
    else {
        child_thread->t_ppid = curthread->t_pid;
    }
	child_thread->t_exitflag = 0; 
	child_thread->t_exitcode = -25;

    /* Create locking device for wait_pid */
    child_thread->t_exitsem = sem_create("sem for exit...", 0);
    if(child_thread->t_exitsem == NULL) {
        proc_deleteentry(child_pid);
        splx(spl);
        return ENOMEM;
    }

    splx(spl);
    return 0;
}

/* destroy process */
void proc_destroy(struct thread *thread) {
    sem_destroy(thread->t_exitsem);
    /* remove process from the table */
    proc_deleteentry(thread->t_pid);
}


/********************************************************/
/* Following functions are helpers for the system calls */
/********************************************************/

/* Create a process fork by using thread fork, and jumping to the machine dependant md_forkentry to do a context switch */
int proc_fork(struct trapframe *tf, pid_t *ret_val) 
{
    int err = 0;

    /* Create trap frame for the child and thread object */
    struct trapframe *child_tf = (struct trapframe *)kmalloc(sizeof(struct trapframe));
    if(child_tf == NULL) {
        //proc_exit(1);       /* For forkbomb? */
        return ENOMEM;
    }
    *child_tf = *tf;

    /* copy parent space into new child address space */
    struct addrspace *child_addrspace;
    err = as_copy(curthread->t_vmspace, &child_addrspace);
    if(err) {
        kfree(child_tf);
        return err;
    }

    /* Check to see if there are any pids available */
    if(!proc_pid_avail()) {
        return EAGAIN;
    }

    /* create a fork, child thread is set up to start from md_forkentry */
    struct thread *child_thread;
    err = thread_fork("dont care", (void *)child_tf, (unsigned long)child_addrspace, md_forkentry, &child_thread);
    if(err) {
        as_destroy(child_addrspace);
        kfree(child_tf);
        return err;
    }

    /* return */
    *ret_val = child_thread->t_pid;
    return 0;
}


/* 
 * Given an exitcode, change the process information.
 * We also want to signal the threads waiting on this one, we do this by using the t_exitsem 
 */
void proc_exit(int exitcode) 
{
    /* Disable interrupts, no interruptions for exiting! */
    int spl = splhigh();

    curthread->t_exitcode = exitcode;
    curthread->t_exitflag = 1;

    V(curthread->t_exitsem);        // Now others waiting for this pid can continue with their lives

    /* Apparently we need to change the status of this processes children... ADOPTION!! */
    int index;
    struct thread *child;
    for(index=1; index<MAX_PID; index++) {
        child = process_table[index];
        if(child != NULL) {
            if(curthread->t_pid == child->t_ppid) {
                child->t_ppid = 1;
                child->t_adoptedflag = 1;
            }
        }
    }

    splx(spl);
    thread_exit();
}


/* Helper function for sys_wait */
int proc_wait(int pid, int *exitcode)
{
    /* Turn off interrupts */
    int spl = splhigh();
    if( pid >= MAX_PID || pid <= 0 ){
        *exitcode = 0;
        splx(spl);
        return EINVAL;
    }
    
    struct thread *current = process_table[pid];
    if ( current == NULL ){
        *exitcode = 0;
        splx(spl);
        return EINVAL;
    }
    assert(current->t_pid == pid);

    /* Check if waiting only on the pid of the children */
    if( current->t_ppid != curthread->t_pid){
        *exitcode = 0;
        splx(spl);
        return EINVAL;
    }

    if( current->t_exitflag ){
        *exitcode = current->t_exitcode;
        proc_reap(pid);
        splx(spl);
        return 0;
    }
    else {
        /* Wait on the lock until the child terminates its execution */ 
        P(current->t_exitsem);
        /* Call process and thread reaping function */
        *exitcode = current->t_exitcode;
        proc_reap(pid);
        splx(spl);
        return 0;
    }
}

/* reap a process, remove it from process table and zombie array */
void proc_reap(int pid){
    struct thread* to_reap = process_table[pid];
    assert(to_reap->t_pid == pid);

    int idx;
    for (idx=0; idx<array_getnum(zombies); idx++) {
		struct thread *to_remove = array_getguy(zombies, idx);
        if( to_remove->t_pid == pid ){
            array_remove(zombies, idx);
            proc_destroy(to_reap);
            thread_destroy(to_reap);
            return;
        }
	}
}
/*
 * Creates new process and makes it execute a different program
 */ 
int proc_execv(char *pathname_k, char **argv, int size_args){
	struct vnode *v;
	vaddr_t entrypoint, stack_ptr;
	int result;

	/* Open the file. */
	result = vfs_open(pathname_k, O_RDONLY, &v);
	if (result) {
		return result;
	}
    
	/* Save the old address space */
	assert(curthread->t_vmspace != NULL);
    struct addrspace *old_add = curthread->t_vmspace;
    curthread->t_vmspace = NULL;

	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
        /* Reassing the current address space to the old one before returning */
        curthread->t_vmspace = old_add;
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint); // need to implement load_elf differently 
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
        as_destroy(curthread->t_vmspace);
        curthread->t_vmspace = old_add;
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	//vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stack_ptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
        as_destroy(curthread->t_vmspace);
        curthread->t_vmspace = old_add;
		return result;
	}

    as_destroy(old_add);

    char **temp = kmalloc( sizeof(char *) * size_args );

    int index;
    int len;
    for( index = 0; index < size_args; index++ ){
        len = strlen(argv[index]) + 1;
        stack_ptr = stack_ptr - len;
        result = copyout((const void *)argv[index], (userptr_t)(stack_ptr), (size_t)len);
        if( result ){
            kfree(temp);
            return result;
        } 
        temp[index] = (char *)stack_ptr;   
    }

    stack_ptr = stack_ptr - stack_ptr % 4;

    stack_ptr = stack_ptr - (sizeof(char *) * size_args);

    result = copyout((const void *)temp, (userptr_t)stack_ptr, (size_t)(sizeof(char *) * size_args));
    kfree(temp);
    if( result ){
        return result;
    }

    // maybe here we need to free argv completely
    int i;
    for( i=0; i < size_args; i++ ){
        kfree(argv[i]);
    }
    kfree(argv);

	/* Warp to user mode. */
	md_usermode((int)size_args, (userptr_t)stack_ptr,
		    stack_ptr, entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;    
}
