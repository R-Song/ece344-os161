
/*
 * The non dumb version of os161 vm implementation. The functions in this file are called by
 * kmalloc() and kfree() and are absolutely critical for the OS to function.
 * In fact, the OS won't even boot if these are not implemented.
 *
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>

/*
 *
 */
void
vm_bootstrap(void)
{
	/* do nothing */
}

/*
 *
 */
vaddr_t 
alloc_kpages(int npages)
{
	/*
	 * Write this.
	 */
	
	(void)npages;
	return 0;
}

void 
free_kpages(vaddr_t addr)
{
	/*
	 * Write this.
	 */

	(void)addr;
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	/*
	 * Definitely write this.
	 */

	(void)faulttype;
	(void)faultaddress;
	return 0;
}
