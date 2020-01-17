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
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

/*
 * alloc_kpages() and free_kpages() are called by kmalloc() and thus the whole
 * kernel will not boot if these 2 functions are not completed.
 */

void
vm_bootstrap(void)
{
	/* do nothing */
}

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
