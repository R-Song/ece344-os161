#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <lib.h>
#include <machine/pcb.h>
#include <machine/spl.h>
#include <machine/trapframe.h>
#include <kern/callno.h>
#include <syscall.h>
#include <clock.h>


/*
 * System call handler.
 *
 * A pointer to the trapframe created during exception entry (in
 * exception.S) is passed in.
 *
 * The calling conventions for syscalls are as follows: Like ordinary
 * function calls, the first 4 32-bit arguments are passed in the 4
 * argument registers a0-a3. In addition, the system call number is
 * passed in the v0 register.
 *
 * On successful return, the return value is passed back in the v0
 * register, like an ordinary function call, and the a3 register is
 * also set to 0 to indicate success.
 *
 * On an error return, the error code is passed back in the v0
 * register, and the a3 register is set to 1 to indicate failure.
 * (Userlevel code takes care of storing the error code in errno and
 * returning the value -1 from the actual userlevel syscall function.
 * See src/lib/libc/syscalls.S and related files.)
 *
 * Upon syscall return the program counter stored in the trapframe
 * must be incremented by one instruction; otherwise the exception
 * return code will restart the "syscall" instruction and the system
 * call will repeat forever.
 *
 * Since none of the OS/161 system calls have more than 4 arguments,
 * there should be no need to fetch additional arguments from the
 * user-level stack.
 *
 * Watch out: if you make system calls that have 64-bit quantities as
 * arguments, they will get passed in pairs of registers, and not
 * necessarily in the way you expect. We recommend you don't do it.
 * (In fact, we recommend you don't use 64-bit quantities at all. See
 * arch/mips/include/types.h.)
 */

void
mips_syscall(struct trapframe *tf)
{
	int callno;
	int32_t retval;
	int err;

	assert(curspl==0);

	callno = tf->tf_v0;

	/*
	 * Initialize retval to 0. Many of the system calls don't
	 * really return a value, just 0 for success and -1 on
	 * error. Since retval is the value returned on success,
	 * initialize it to 0 by default; thus it's not necessary to
	 * deal with it except for calls that return other values, 
	 * like write.
	 */

	retval = 0;

	switch (callno) {
	    case SYS_reboot:
			err = sys_reboot(tf->tf_a0);
		break;
	    /* Add stuff here */
		case SYS__exit:
			
		break;

		case SYS_write:
			err = sys_write( (int)tf->tf_a0, (const void *)tf->tf_a1, (size_t)tf->tf_a2, &retval );
		break;

		case SYS_read:
			err = sys_read( (int)tf->tf_a0, (void *)tf->tf_a1, (size_t)tf->tf_a2, &retval );
		break;

		case SYS_sleep:
			err = sys_sleep( (unsigned int)tf->tf_a0 );
		break;

		case SYS___time:
			err = sys___time( (time_t *)tf->tf_a0, (unsigned long *)tf->tf_a1, &retval );
		break;

	    default:
			kprintf("Unknown syscall %d\n", callno);
			err = ENOSYS;
		break;
	}


	if (err) {
		/*
		 * Return the error code. This gets converted at
		 * userlevel to a return value of -1 and the error
		 * code in errno.
		 */
		tf->tf_v0 = err;
		tf->tf_a3 = 1;      /* signal an error */
	}
	else {
		/* Success. */
		tf->tf_v0 = retval;
		tf->tf_a3 = 0;      /* signal no error */
	}
	
	/*
	 * Now, advance the program counter, to avoid restarting
	 * the syscall over and over again.
	 */
	
	tf->tf_epc += 4;

	/* Make sure the syscall code didn't forget to lower spl */
	assert(curspl==0);
}

void
md_forkentry(struct trapframe *tf)
{
	/*
	 * This function is provided as a reminder. You need to write
	 * both it and the code that calls it.
	 *
	 * Thus, you can trash it and do things another way if you prefer.
	 */

	(void)tf;
}


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
	switch( fd ) 
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
unsigned int sys_sleep(unsigned int seconds)
{
    clocksleep(seconds);
    
    return 0;
}


/* 
 * System call for __time
 */
time_t sys___time(time_t *seconds, unsigned long *nanoseconds, int *retval)
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


