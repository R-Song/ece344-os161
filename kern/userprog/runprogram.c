/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>
#include <vm_features.h>


/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char **argv, unsigned long size_args)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	assert(curthread->t_vmspace == NULL);

	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Load the executable. */
	if(LOAD_ON_DEMAND_ENABLE){
		result = load_elf_od(v, &entrypoint);
	}
	else {
		result = load_elf(v, &entrypoint);
	}
	
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		return result;
	}

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		return result;
	}
    
	char **temp = kmalloc( sizeof(char *) * size_args );

    unsigned long index;
    int len;
    for( index = 0; index < size_args; index++ ){
        len = strlen(argv[index]) + 1;
        stackptr = stackptr - len;
        result = copyout((const void *)argv[index], (userptr_t)(stackptr), (size_t)len);
        if( result ){
            kfree(temp);
            return result;
        } 
        temp[index] = (char *)stackptr;   
    }

    stackptr = stackptr - stackptr % 4;

    stackptr = stackptr - (sizeof(char *) * size_args);

    result = copyout((const void *)temp, (userptr_t)stackptr, (size_t)(sizeof(char *) * size_args));
    kfree(temp);
    if( result ){
        return result;
    }

	/* Warp to user mode. */
	md_usermode((int)size_args, (userptr_t)stackptr,
		    stackptr, entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}

