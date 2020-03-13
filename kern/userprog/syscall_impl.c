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

/*
 * System call for write.
 * Takes file descriptor, user mode buffer, and requested number of bytes as inputs
 * Returns err number. Returns the number of bytes written to retval. If write fails, return -1 through retval
 */
int
sys_write(int fd, const void *buf, size_t nbytes, int *retval)
{	
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
		return EFAULT;
	}
	kbuf[nbytes] = '\0'; // pad with NULL character

	/* Check file descriptor, only handles stdout and stderr */
	switch (fd) 
	{
		case STDOUT_FILENO:
			*retval = kprintf("%s", kbuf);
			kfree(kbuf);
			return 0;
		break;

		case STDERR_FILENO:
			/* How to distinguish stdout from stderr??? */
			*retval = kprintf("%s", kbuf);
			kfree(kbuf);
			return 0;
		break;
		
		default:
			/* Neither stdout or stderr, cannot handle this */
			*retval = -1;
			kfree(kbuf);
			return EBADF;
		break;
	}

	/* Should not reach here */
	return 0;
}


/*
 * System call for read.
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
 * As an input, it takes the entire trapframe from the exception. Using this trapframe, a new thread is created that
 * has identical states, but different memory space and file tables. 
 * File handle objects are, however, shared.
 * 
 * Returns pid of child process to caller
 * Returns 0 to child process
 * 
 * Exact mechanism for return is to put the value into the return register
 *  
 * Possible errors:
 * 		EAGAIN: Too many processes exist
 * 		ENOMEM: Not enough virtual memory to accomodate this process
 */
int sys_fork(struct trapframe *tf, pid_t *ret_val) 
{
	int child_pid;	
	
	int err = proc_fork(tf, &child_pid);
	if(err) {
		*ret_val = -1;
		return err;
	}

	*ret_val = child_pid;
	return 0;
}

/* Get PID */
int sys_getpid(pid_t *retval) {
    *retval = curthread->t_pid;
    return 0;
}

/* System call for exit. */ 
int sys__exit(int exitcode)
{	
	proc_exit(exitcode);
	panic("Should not return from exit...EVER!!");
	return 0;
}

/* System call for wait pid */
int sys_waitpid(int pid, int *status, int options, int *retval)
{
	int exitcode;
	if( options ){
		*retval = -1;
		return EINVAL;	
	}
	int err;
	err = proc_wait(pid, &exitcode);
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
 * execv replaces currently executing program with a newly loaded program image
 * this occurs within one process; process id is unchanged
 * 
 * pathname of the program to run is passed as program
 * args is an array of 0-terminated strings; the array itself should be terminated by a NULL ptr
 * 
 * the argument strings should be copied into the new process as the new process' argv[] array
 * in the new process, argv[arg] must be NULL
 * 
 * argv[0] in the new process contains the name that was used to invoke the program
 * this is not necessarily the same as program, and is only a convention and should not be enforced by the kernel
 * 
 * process file table and current workind directory are not modified by execv
 * 
 * Return values:
 * on success, nada, the new programming is executing
 * on failure, returns -1 and sets erro to:
 * ENODEV	The device prefix of program did not exist. - probabably done in proc_execv
 * ENOTDIR	A non-final component of program was not a directory. - probabably done in proc_execv
 * ENOENT	program did not exist. - here
 * EISDIR	program is a directory. 
 * ENOEXEC	program is not in a recognizable executable file format, was for the wrong platform, or contained invalid fields. - proc
 * ENOMEM	Insufficient virtual memory is available. - proc
 * E2BIG	The total size of the argument strings is too large. - here
 * EIO	A hard I/O error occurred.
 * EFAULT	One of the args is an invalid pointer.
 * 
 */ 
int sys_execv(const char *program, char **args, pid_t *retval){
	int err = 0;
	int actual_size; // does it need to be a pointer????

	/* Check if program exists */
	if( program == NULL ){
		*retval = -1;
		return ENOENT;
	}

	/* Check if one of the args is invalid */
	if( args == NULL ){
		*retval = -1;
		return EFAULT;
	}
	/* Check if pointer is valid */
	char *valid_arr;
	err = copyin( (const_userptr_t)args, (void *)&valid_arr, sizeof(char **));
	if( err ){
		*retval = -1; 
		return err;
	}
	/* Size of args array */
	int size_args = 0;
	while ( args[size_args] != NULL ){
		size_args = size_args + 1;
	}

	/* Get the length of the args array and use it for allocating space for the kernel array in which you will copy into */
	char **argv = kmalloc( size_args * sizeof(char *));
	int args_str, del_from;

	/* Copy the contents of the args array into argv */
	for( args_str = 0; args_str < size_args; args_str++ ){
		/* Allocate space for each string in argv array */
		argv[args_str] = kmalloc(sizeof(char *) * (NAME_MAX + 1));
		err = copyinstr(( const_userptr_t )program, argv[args_str], (NAME_MAX + 1), &actual_size); // add + 1 to name_max??
		if( err ){
			/* Free everything allocated until current args element (all of argv) and return failure*/
			for( del_from = 0; del_from < args_str; del_from++ ){
				kfree(argv[del_from]);
			}
			kfree(argv);
			*retval = -1;
			return err;
		}
	}
	
	char pathname_k[PATH_MAX];

	/* Checks if the pathname exceeds pathname max length or if the pathname has run into the end of the userspace */
	err = copyinstr(( const_userptr_t )program, pathname_k,  PATH_MAX, &actual_size);
	if( err ){
		*retval = -1;
		return err;
	}

	/* Create the new address space, loading the executable and other similar runprogram actions are done in proc_execv*/
	err = proc_execv(pathname_k, argv, size_args);

	return 0;
}

