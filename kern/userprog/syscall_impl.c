/*
 * Implementation of a few system calls that are general to all programs. 
 * Following implemented system calls are the following:
 *      sys_write(), sys_read(), sys_sleep(), sys__time()
 * 		sys_fork(), sys_execv(), sys_getpid(), sys_waitpid(), sys__exit()
 * 
 * Refer to the Man pages for more information
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/limits.h>
#include <lib.h>
#include <kern/callno.h>
#include <clock.h>
#include <syscall.h>
#include <thread.h>
#include <curthread.h>
#include <process.h>
#include <addrspace.h>
#include <pagetable.h>
#include <coremap.h>
#include <vm.h>
#include <machine/spl.h>
#include <swap.h>
#include <synch.h>

/*
 * System call for write.
 * 
 * Takes file descriptor, user mode buffer, and requested number of bytes as inputs
 * Returns err number. Returns the number of bytes written to retval. If write fails, return -1 through retval
 */
int
sys_write(int fd, const void *buf, size_t nbytes, int *retval)
{	
	int spl = splhigh();

	/* 
	 * Allocate kernel memory and attempt to copy user buffer to kernel buffer
	 * Note, the buffer may not be null terminated, therefore we need to append a null character
	 */
	char *kbuf = (char *)kmalloc( (nbytes+1) * sizeof(char) );
	int err = copyin(buf, kbuf, nbytes);
	if(err)
	{
		*retval = -1;
		kfree(kbuf);
		splx(spl);
		return EFAULT;
	}
	kbuf[nbytes] = '\0'; // pad with NULL character

	/* Check file descriptor, only handles stdout and stderr */
	switch (fd) 
	{
		case STDOUT_FILENO:
			*retval = kprintf("%s", kbuf);
			kfree(kbuf);
			splx(spl);
			return 0;
		break;

		case STDERR_FILENO:
			/* How to distinguish stdout from stderr??? */
			*retval = kprintf("%s", kbuf);
			kfree(kbuf);
			splx(spl);
			return 0;
		break;
		
		default:
			/* Neither stdout or stderr, cannot handle this */
			*retval = -1;
			kfree(kbuf);
			splx(spl);
			return EBADF;
		break;
	}

	/* Should not reach here */
	panic("Should not reacher here - sys_write");
	return 0;
}


/*
 * System call for read.
 * 
 * Calls kgets_sys_read which is located in kgets.c
 * System calls takes file descriptor, max buffer length, and return value pointer as arguments
 * Returns 0 if success and appropriate error code if error occurs. Retval variable returns -1 if failed, and the length of the read if successful
 */
int sys_read(int fd, void *buf, size_t buflen, int *retval)
{
	/* Allocate Kernel memory to store input from console */
	char *kbuf = (char *)kmalloc( (buflen+1)*sizeof(char) );
	int kbuflen = buflen + 1;

	/* Currently only handles read from standard input, can expand switch statement later on to accomodate more */
	switch(fd) 
	{
		case STDIN_FILENO:
			kgets_sys_read(kbuf, kbuflen);
		break;

		default:
			kfree(kbuf);
			*retval = -1; // read has failed
			return EBADF;
		break;
	}

	/* Attempt to copy from kernel buffer to user buffer */
	int err = copyout( kbuf, buf, buflen );
	if(err) 
	{
		kfree(kbuf);
		*retval = -1; // read has failed
		return EFAULT;
	}

	/* Copy was successful, return the length of string read */
	int length = strlen(kbuf);
	kfree(kbuf);
	*retval = length;
	return 0;
}


/*
 * System call for sleep.
 */
int sys_sleep(unsigned int seconds)
{
    clocksleep(seconds);
    return 0;
}


/* 
 * System call for __time
 */
int sys___time(time_t *seconds, unsigned long *nanoseconds, time_t *retval)
{ 
    time_t k_dest_sec;
    unsigned long k_dest_nanosec;
    
    time_t sec;
    unsigned long nanosec;
    
    int err_sec = copyin((const_userptr_t)seconds, &k_dest_sec, sizeof(seconds));
    int err_nanosec = copyin((const_userptr_t)nanoseconds, &k_dest_nanosec, sizeof(seconds));

    if( (seconds != NULL && err_sec) || (nanoseconds != NULL && err_nanosec) )
    {
        *retval = -1;
        return EFAULT;
    }
    
    if( seconds == NULL )
    {
        if(nanoseconds == NULL)
        {
            gettime(&sec, (u_int32_t *)&nanosec);
            *retval = (int32_t)sec;    
        }
        else
        {
            gettime(&sec, (u_int32_t *)nanoseconds);
            *retval = (int32_t)sec;
        }
    
    }
    else if( nanoseconds == NULL)
    {   
        if( seconds == NULL )
        {
            gettime(&sec, (u_int32_t *)&nanosec);
            *retval = (int32_t)sec;
        }
        else
        {
            gettime(seconds, (u_int32_t *)&nanosec);
            *retval = (int32_t)*seconds;     
        }
    }
    else {
        gettime(seconds, (u_int32_t *)nanoseconds);
        *retval = (int32_t)*seconds;
    }

	return 0;
}


/*
 * System call for fork.
 * 
 * As an input, it takes the entire trapframe from the exception. Using this trapframe, a new thread is created that
 * has identical states, but different memory space and file tables. 
 * File handle objects are, however, shared.
 * 
 * Takes 1 parameter:
 * 		The current hardware state of the process, aka the trapframe
 * 
 * Returns:
 * 		1. pid of child process to caller
 * 		2. function returns 0 to child process
 *		Exact mechanism for return is to put the value into the return register
 *  
 * Valid error codes to return:
 * 		EAGAIN: Too many processes exist
 * 		ENOMEM: Not enough virtual memory to accomodate this process
 * 
 * Strategy for implementation:
 * 		1. Call proc_fork()
 */
int sys_fork(struct trapframe *tf, pid_t *ret_val) 
{
	int spl = splhigh();
	lock_acquire(swap_lock);

	int child_pid;	
	
	int err = proc_fork(tf, &child_pid);
	if(err) {
		*ret_val = -1;
		return err;
	}
	*ret_val = child_pid;

	lock_release(swap_lock);
	splx(spl);
	return 0;
}


/* 
 * System call for getpid.
 * 
 * Straightforward enough, returns the curthread's pid
 * getpid does not fail, hence does not return errors
 */
int sys_getpid(pid_t *retval) {
    *retval = curthread->t_pid;
    return 0;
}


/* 
 * System call for waitpid.
 * 
 * This system call is tied closely to the exit system call.
 * This is because when a thread exits, it becomes a zombie. It is then up to the parent to reap
 * its child process, thus removing it from the process table and recovering that process's exitcode
 * 
 * Takes 3 parameters:
 * 		1. pid, must be one of this process's children, otherwise it is invalid
 * 		2. *status, this is the pointer which we will copy the exitcode of the child process into.
 * 		3. options, don't worry about this, just check to make sure it is 0
 * 
 * Returns:
 * 		1. retval, returns -1 if pid passed in is not a child, status is not a valid pointer, or options is not 0
 * 		   there are other cases, but those are just a few.
 * 		2. function itsself returns the error code.
 * 	
 * Valid error codes to return:
 * 		EINVAL	The options argument requested invalid or unsupported options.
 * 		EFAULT	The status argument was an invalid pointer.			
 * 
 * Strategy for implementation:
 * 		1. Check to make sure arguments are valid.
 * 		2. Call proc_waitpid() to do the heavy lifting.
 */
pid_t sys_waitpid(pid_t pid, int *status, int options, int *retval)
{
	int exitcode;
	int err;

	if( options ){
		*retval = -1;
		return EINVAL;	
	}

	err = proc_waitpid(pid, &exitcode);
	if( err ){
		*retval = -1;
		return err;
	}

	/* make sure exit code changed. -25 is a magic number :) */
	assert(exitcode != -25);

	err = copyout( &exitcode, (userptr_t)status, sizeof(int));
	if( err ){
		*retval = -1;
		return err;
	}

	/* if no errors, return the pid */
	*retval = pid;
	return 0;
}


/* 
 * System call for exit. 
 * 
 * Exit basically kills a thread and makes it a zombie.
 * The last bit of information that remains is the exitcode, which the parent can retrieve
 * through the waitpid system call. To communicate to a thread that is waiting
 * there is a exitflag as well as a semaphore which signals waiters.
 * 
 * Takes one parameter:
 * 		The exitcode
 * 
 * Returns:
 * 		Should not return. If exit returns, then we panic...
 * 
 * Strategy for implementation:
 * 		Call proc_exit()
 */ 
int sys__exit(int exitcode)
{	
	proc_exit(exitcode);
	panic("Should not return from exit...EVER!!");
	return 0;
}


/*
 * System call for execv.
 * 
 * Takes two parameters: 
 * 		1. pointer to the string that represents the program path
 * 		2. array of null terminated strings that hold the arguments of the program.
 * 		   The array itsself should also be terminated by a NULL pointer
 * 
 * Returns two values only if execv fails:
 * 		1. retval returns -1 if failed, otherwise exec should not return
 * 		2. function returns errno to be handled by mips_syscall()
 * 		NOTE: On success execv should NOT return as the next program is executing!
 * 
 * Valid Error codes to be returned:
 * 
 * ENODEV	The device prefix of program did not exist.
 * ENOTDIR	A non-final component of program was not a directory.
 * ENOENT	program did not exist.
 * EISDIR	program is a directory. 
 * ENOEXEC	program is not in a recognizable executable file format, was for the wrong platform, or contained invalid fields. 
 * ENOMEM	Insufficient virtual memory is available.
 * E2BIG	The total size of the argument strings is too large.
 * EIO	A hard I/O error occurred.
 * EFAULT	One of the args is an invalid pointer.
 * 
 * Strategy for implementation:
 * 		1. Arguments are copied from user space into kernel space and checked for validity
 * 		2. Once arguments are valid, call proc_execv() to handle the rest
 */

/* Maximum program length */
const int MAX_ARGLEN = 64;		// maximum number of characters in an argument
const int MAX_ARGNUM = 32;		// maximum number of arguments

int sys_execv(const char *user_program, char **args, pid_t *retval){
	int spl = splhigh();

	int err = 0; 		/* Error code, if exists */
	int argc = 0;		/* Number of arguments, gotta count the length of args */
	char **argv;		/* dynamically allocate the array once we have a grasp at what argc is */
	char *program;		/* we have to copy the user_program string into program using copystr */

	/* Check if user_program is valid */
	program = kmalloc(MAX_ARGLEN*sizeof(char));	/* allocate kernel memory for program name, probably allocating too much, but that's fine */
	int program_len = -1;
	err = copyinstr( (const_userptr_t) user_program, program, MAX_ARGLEN, &program_len);
	if(err) {
		kfree(program);
		goto execv_failed;
	}

	/* Gotta pass badcall!! */
	/* Check if arg contains at least one thing */
	if(args == NULL) {
		err = EFAULT;
		goto execv_failed;
	}
	/* Check to see if args is a valid pointer */
	char *test_valid_pointer;
	err = copyin( (const_userptr_t)args, (void *)&test_valid_pointer, sizeof(char **) );
	if(err) {
		kfree(program);
		goto execv_failed;
	}

	/* generate argv */
	argv = kmalloc(MAX_ARGNUM*sizeof(char *));
	int arg_len;
	int idx;

	for(idx=0; idx<MAX_ARGNUM; idx++) {
		if(args[idx] != NULL) {
			argc++;
			argv[idx] = kmalloc(MAX_ARGLEN*sizeof(char));
			err = copyinstr( (const_userptr_t)args[idx], argv[idx], MAX_ARGLEN, &arg_len );

			/* If there is an error, deallocate everything */
			if(err) {
				int temp;
				for(temp=0; temp<=idx; temp++) {
					kfree(argv[temp]);
				}
				kfree(argv);
				kfree(program);
				goto execv_failed;
			}
		} else {
			argv[argc] = NULL;		/* Terminate argv with a null pointer */
			break;
		}
		/* too many arguments? */
		if(idx == MAX_ARGNUM-1) {
			int temp;
			for(temp=0; temp < MAX_ARGNUM; temp++) {
				kfree(argv[temp]);
			}
			kfree(argv);
			kfree(program);
			err = E2BIG;
			goto execv_failed;
		}
	}
	assert(argv[argc] == NULL);		/* This must be true */
	
	/* argc and argv have been prepared properly, time to launch proc_execv */
	err = proc_execv(program, argc, argv);
	if(err) {
		/* deallocation should be done inside proc_exec() */
		goto execv_failed;
	}

execv_failed:
	*retval = -1;
	splx(spl);
	return err;
}

/*
 * The "break" is the end address of a process's heap region
 * The sbrk call adjusts the "break" by the amount amount. It returns the old "break"
 * 
 * Returns:
 *  On success, sbrk returns the previous value of the "break". 
 *  On error, ((void *)-1) is returned, and errno is set according to the error encountered.
 * 
 * Valid errors to return:
 * 	ENOMEM	Sufficient virtual memory to satisfy the request was not available, or the process has reached the limit of the memory it is allowed to allocate.
 *  EINVAL	The request would move the "break" below its initial value.
 * 
 * Strategy for implementation:
 * 	Round to the nearest page, then allocate space on the heap accordingly.
 */
#if !OPT_DUMBVM

int sys_sbrk(intptr_t amount, pid_t *retval)
{
	int spl = splhigh();
	lock_acquire(swap_lock);

	size_t i;
	struct pte *new_entry;
	vaddr_t vaddr;
	int err;

	/* Retrieve current address space */
	struct addrspace *as = curthread->t_vmspace;
	assert(as != NULL);								/* This is a user process, it must have an addrspace */
	vaddr_t heapstart = as->as_heapstart;
	vaddr_t old_heapend = as->as_heapend;
	size_t old_heapsize = ((old_heapend - heapstart + PAGE_SIZE-1) >> PAGE_OFFSET); /* size of heap in pages */

	/* Ensure that amount is not too negative. The operations looks weird because we are working with unsigned values */
	if( (amount < 0) && (old_heapend - heapstart < (unsigned)amount*-1) ){
		*retval = -1;
		lock_release(swap_lock);
		splx(spl);
		return EINVAL;	
	} 

	/* Anything more than this is too large for one allocation */
	if(amount > 8*8192) {
		*retval = -1;
		lock_release(swap_lock);
		splx(spl);
		return ENOMEM;
	}

	/* Check if we should be allocating or freeing memory */
	if(amount == 0) 
	{
		*retval = old_heapend;
		lock_release(swap_lock);
		splx(spl);
		return 0;
	}
	else if(amount > 0) 
	{
		/* Let vm_fault allocate pages on demand */
		// as->as_heapend += amount;
		// *retval = old_heapend;
		// lock_release(swap_lock);
		// splx(spl);
		// return 0;

		/* Check to see if allocation requires a new page */
		vaddr_t new_heapend = old_heapend + amount;
		vaddr_t new_heapsize = ((new_heapend - heapstart + PAGE_SIZE-1) >> PAGE_OFFSET);

		if(new_heapsize == old_heapsize) 
		{
			as->as_heapend += amount;
			*retval = old_heapend;
			lock_release(swap_lock);
			splx(spl);
			return 0;
		}
		else
		{
			size_t num_pages_requested = new_heapsize - old_heapsize;
			/* Allocate the pages requested by the user */
			for(i=0; i<num_pages_requested; i++) {
				vaddr = ((old_heapend + i*PAGE_SIZE + PAGE_SIZE-1) & PAGE_FRAME);

				new_entry = pte_init();
				if(new_entry == NULL) {
					goto sbrk_failed;
				}

				err = swap_allocpage_od(new_entry);
				if(err) {
					pte_destroy(new_entry);
					goto sbrk_failed;
				}
				new_entry->permissions = set_permissions(1, 1, 0);

				pt_add(as->as_pagetable, vaddr, new_entry);
			}
			/* Update heap breakpoint */
			as->as_heapend += amount;
			*retval = old_heapend;
			lock_release(swap_lock);
			splx(spl);
			return 0;

		sbrk_failed:
			/* Allocate failed along the way. Free all allocated pages */
			for(i=0; i < num_pages_requested; i++){
				vaddr = ((old_heapend + i*PAGE_SIZE + PAGE_SIZE-1) & PAGE_FRAME);
				new_entry = pt_get(as->as_pagetable, vaddr);
				if(new_entry == NULL) {
					continue;
				}

				free_upage(new_entry);
				pt_remove(as->as_pagetable, vaddr);
			}
			*retval = -1;
			lock_release(swap_lock);
			splx(spl);
			return ENOMEM;	
		}
	}
	else if(amount < 0)
	{
		/* Check to see if deallocation allows us to free a page */
		vaddr_t new_heapend = old_heapend + amount;
		size_t new_heapsize = ((new_heapend - heapstart + PAGE_SIZE-1) >> PAGE_OFFSET);
		if(new_heapsize == old_heapsize) {
			as->as_heapend += amount;
			*retval = old_heapend;
			lock_release(swap_lock);
			splx(spl);
			return 0;
		}
		else
		{
			size_t num_pages_dealloc = old_heapsize - new_heapsize;

			for(i=0; i<num_pages_dealloc; i++) {
				vaddr = ((old_heapend - i*PAGE_SIZE) & PAGE_FRAME);
				new_entry = pt_get(as->as_pagetable, vaddr);

				if(new_entry == NULL) {
					continue;
				}

				free_upage(new_entry);
				pt_remove(as->as_pagetable, vaddr);
			}

			as->as_heapend += amount;
			*retval = old_heapend;
			lock_release(swap_lock);
			splx(spl);
			return 0;
		}
	}
	panic("Should not reach here");
	return 0;
}

#endif
