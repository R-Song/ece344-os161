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
#include <pagetable.h>
#include <vm_features.h>

/* Lock to synchronize the process table */
static struct lock *process_lock;


/********************************************************************************************/
/* Functions that deal with process lookup table, handling PID allocation and reclaimation, */
/*              process initialization, destruction, and reaping                            */
/********************************************************************************************/

/* constant for max number of threads */
const int MAX_PID = 150;

/* Global variable holding process lookup table */
struct thread **process_table;

/* Redeclare zombies array used in thread.c */
extern struct array *zombies;


/* Initialize process table */
void proc_bootstrap() 
{
    process_lock = lock_create("Process Lock");
    if(process_lock == NULL) {
        panic("Could not create process lock \n");
    }

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


/* 
 * Add an entry - returns the PID, if no more PID's exist return EAGAIN. 
 * Index starts at 1 because PID 0 is reserved for swapper or scheduler. 
 */
int proc_addentry(struct thread *thread, pid_t *retval)
{   
    assert(curspl>0);
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
    assert(curspl>0);
    pid_t index;
    for( index=1; index<MAX_PID; index++ ) {
        if(process_table[index] == NULL) {
            return 1;
        }
    }
    return 0;
}


/* Delete an entry from the process table */
void proc_deleteentry(pid_t pid)
{
    assert(curspl>0);
    process_table[pid] = NULL;
}


/* 
 * Initialize thread fields pertaining to the process, this function is called in thread_create(), 
 * which is called in thread_fork(), which is called in proc_fork() , hench why its in this file :)
 */
int proc_init(struct thread *child_thread) {

    assert(curspl>0);

    /* pid allocation */
    pid_t child_pid;
    int err = proc_addentry(child_thread, &child_pid);
    if(err) {
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
    child_thread->t_adoptedflag = 0;
	child_thread->t_exitcode = -25;
    child_thread->t_waitflag = 0;

    /* Create locking device for wait_pid */
    child_thread->t_exitsem = sem_create("sem for exit...", 0);
    if(child_thread->t_exitsem == NULL) {
        proc_deleteentry(child_pid);
        return ENOMEM;
    }

    return 0;
}


/* 
 * Destroy information related to the process 
 * We only do this when are reaping a process
 */
void proc_destroy(struct thread *thread) {
    sem_destroy(thread->t_exitsem);
    proc_deleteentry(thread->t_pid);
}


/* 
 * Reap a process, remove it from process table as well as the zombie array
 * Only the parent of a process can reap it, so this is naturally called in proc_waitpid
 */
void proc_reap(int pid){
    assert(curspl>0);

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


/* Shutdown process */
void proc_shutdown() {
    kfree(process_table);
}


/* Function for debugging, prints entire process table */
void proc_stat() {
    int spl = splhigh();
    int i;
    int j=0;
    for(i=0; i<MAX_PID; i++) {
        kprintf("--PID:%d | PPID:%d--", i, process_table[i]->t_ppid);
        if(j > 9){
            kprintf("\n");
            j=0;
        }
        j++;
    }
    splx(spl);
}



/********************************************************/
/* Following functions are helpers for the system calls */
/********************************************************/

/* 
 * Helper function for sys_fork().
 * 
 * Strategy for implementation:
 *      1. Create a trap frame for the child that is the same as current trapframe
 *      2. Create new address space for the child
 *      3. Assign the child a pid
 *      4. Create a new thread that starts at md_forkentry using thread_fork()
 *         This essentially creates a thread that when scheduled, jumps into userspace
 *         with the same trapframe as this current thread, but a different address space!
 *      5. When thread is created, it is pushed onto the process table and given a proper 
 *         pid along with ppid.
 */
int proc_fork(struct trapframe *tf, pid_t *ret_val) 
{
    int spl;
    int err = 0;
    lock_acquire(process_lock);

    /* Create trap frame for the child and thread object */
    struct trapframe *child_tf = (struct trapframe *)kmalloc(sizeof(struct trapframe));
    if(child_tf == NULL) {
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
        as_destroy(child_addrspace);
        kfree(child_tf);
        return EAGAIN;
    }

    /* create a fork, child thread is set up to start from md_forkentry */
    spl = splhigh();

    struct thread *child_thread;
    err = thread_fork("Child of fork", (void *)child_tf, (unsigned long)child_addrspace, md_forkentry, &child_thread);
    if(err) {
        as_destroy(child_addrspace);
        kfree(child_tf);
        splx(spl);
        return err;
    }  
    child_thread->t_waitflag = 1;
    
    lock_release(process_lock);
    splx(spl);

    /* return */
    *ret_val = child_thread->t_pid;
    return 0;
}


/* 
 * Helper function for sys_wait().
 * 
 * Strategy for implementation: 
 *      1. Check to make sure pid is valid. That is if it is one of current process's children and if
 *         the pid exists on the process table
 *      2. If the child is already exited, return the exit code
 *      3. If the child is not exited, we try spin on the semaphore until the child
 *         process has signaled to us that they are done and have exited
 *      4. We then reap the child process and return the exitcode
 */
int proc_waitpid(pid_t pid, int *exitcode)
{
    /* Turn off interrupts */
    int spl = splhigh();

    lock_acquire(process_lock);

    if( pid >= MAX_PID || pid <= 0 ){
        *exitcode = 0;
        splx(spl);
        lock_release(process_lock);
        return EINVAL;
    }
    
    struct thread *current = process_table[pid];
    if ( current == NULL ){
        *exitcode = 0;
        splx(spl);
        lock_release(process_lock);
        return EINVAL;
    }
    assert(current->t_pid == pid);

    /* Check if waiting only on the pid of the children */
    if( current->t_ppid != curthread->t_pid){
        *exitcode = 0;
        splx(spl);
        lock_release(process_lock);
        return EINVAL;
    }

    if( current->t_exitflag ){
        *exitcode = current->t_exitcode;
        proc_reap(pid);
        splx(spl);
        lock_release(process_lock);
        return 0;
    }
    else {
        /* Wait on the lock until the child terminates its execution */ 
        lock_release(process_lock);
        P(current->t_exitsem);
        /* Call process and thread reaping function */
        lock_acquire(process_lock);
        *exitcode = current->t_exitcode;
        proc_reap(pid);
        lock_release(process_lock);
        splx(spl);
        return 0;
    }
}

/* 
 * Helper function for sys_exit().
 * 
 * Strategy for implementation:
 *      1. Update the t_exitcode and t_exitflag of this current thread
 *      2. Signal waiting processes using the exit semaphore
 *      3. Let pid 1 adopt any of the children that this process has
 *      4. Call thread_exit() to move this thread to a zombie state
 */
void proc_exit(int exitcode) 
{
    /* Disable interrupts, no interruptions for exiting! */
    int spl = splhigh();
    lock_acquire(process_lock);

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

    lock_release(process_lock);
    splx(spl);
    thread_exit();
}


/*
 * Helper function for sys_execv()
 * Refer to syscall_impl.c for the full description of the system call
 * 
 * Strategy for implementation:
 *      1. Create a new address space for the program, we should save the current one in case of failure
 *      2. Activate new address space 
 *      3. Load the ELF file
 *      4. Define the stack in the user space
 *      5. Destroy old address space
 *      6. Push arguments onto the user stack (tricky!)   
 *           (i) Have to watch out for memory alignment
 *           (ii) Have to allocate room for both the actual strings, as well as the pointers to them
 *                This means that we have a bunch of null terminated strings on the stack followed by an array of pointers
 *              0xFFFFFFFF:    ----- strv_n -----
 *                                     .
 *                                     .
 *                                     .
 *                             ----- strv_0 -----
 *                             ----- argv_n -----
 *                                     .
 *                                     .
 *                                     .
 *                             ----- argv_0 -----  -> STACK_PTR   
 *                NOTE: argv_0 is usually the program name, but we don't enforce this
 *      7. Call md_usermode to get into usermode. Should not return from there
 */ 
int proc_execv(char *program, int argc, char **argv){
    int spl = splhigh();

    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    struct addrspace *cur_addrspace;
    int err;
    int idx;

    /* Open the file */
    err = vfs_open(program, O_RDONLY, &v);
    if(err) {
        goto execv_failed;
    }

    /* save current addrspace, use it in case loading a file fails */
    cur_addrspace = curthread->t_vmspace;

    /* create a new address space */
    curthread->t_vmspace = as_create();
    if(curthread->t_vmspace == NULL) {
        vfs_close(v);
        curthread->t_vmspace = cur_addrspace;       /* Restore old addrspace */
        err = ENOMEM;
        goto execv_failed;
    }

    /* activate new addrspace */
    as_activate(curthread->t_vmspace);

    /* load the executable */
    if(LOAD_ON_DEMAND_ENABLE){
        err = load_elf_od(v, &entrypoint);
    }
    else{
        err = load_elf(v, &entrypoint);
    }
    if(err) {
        vfs_close(v);
        as_destroy(curthread->t_vmspace);
        curthread->t_vmspace = cur_addrspace;
        goto execv_failed;
    }   

    /* Define the user stack in the address space */
    err = as_define_stack(curthread->t_vmspace, &stackptr);
    if(err){
        as_destroy(curthread->t_vmspace);
        curthread->t_vmspace = cur_addrspace;
        goto execv_failed;
    }

    /* Push actual strings onto the stack, keep memory aligned! */
    char **user_argv = kmalloc(argc*sizeof(char *));

    int len;
    for( idx = 0; idx < argc; idx++ ){
        len = strlen(argv[idx]) + 1;        /* account for the null termination */
        stackptr -= len;                    /* allocate space on the stack for this string */
        stackptr -= stackptr % 4;           /* important for memory alignment */
        err = copyout((const void *)argv[idx], (userptr_t)(stackptr), (size_t)len);        /* copy out the string to the stack */
        if( err ){
            kfree(user_argv);
            as_destroy(curthread->t_vmspace);
            curthread->t_vmspace = cur_addrspace;
            goto execv_failed;
        } 
        user_argv[idx] = (char *)stackptr;   /* update to point towards the string */
    }
    assert( (stackptr % 4) == 0 );  /* ensure memory alignment */

    /* Push down the stack to accomodate argc */
    stackptr = stackptr - ( argc*sizeof(char *) );

    /* Copy the array over to the stack */
    err = copyout((const void *)user_argv, (userptr_t)stackptr, (size_t) (argc*sizeof(char *)) );
    if(err){
        kfree(user_argv);
        as_destroy(curthread->t_vmspace);
        curthread->t_vmspace = cur_addrspace;
        goto execv_failed;
    }

    /* Deallocate everything */
    kfree(user_argv);
    for(idx=0; idx<=argc; idx++) {
        kfree(argv[idx]);
    }
    kfree(argv);
    kfree(program);
    as_destroy(cur_addrspace);      /* destroy old addrspace */

	/* Warp to user mode. */
    splx(spl);
	md_usermode((int) argc, (userptr_t)stackptr, stackptr, entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;

execv_failed:
    /* Deallocate everything */
    for(idx=0; idx<=argc; idx++) {
        kfree(argv[idx]);
    }
    kfree(argv);
    kfree(program);
    splx(spl);
    return err;
}

