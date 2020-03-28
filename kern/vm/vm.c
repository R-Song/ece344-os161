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

/*
 * Handles TLB faults
 * 
 * TLB faults occur when hardware does not know how to translate
 * a virtual add since the translation is not present in any TLB entry
 * 
 */ 
int
vm_fault(int faulttype, vaddr_t faultaddress)
{	
	/* DON'T USE KPRINTF() IN VM_FAULT() AFTER THE TLB WRITE!!!
	 * 
	 * Three types of faults:
	 * VM_FAULT_READ
	 * VM_FAULT_WRITE
	 * VM_FAULT_READONLY
	 * 
	 * VPN is the search key used for parallel search 
     */
	
	// Check if faultaddress is valid 
	// User code or data segment; user heap between heap_start and heap_end; user stack
	
	// For READ and WRITE
	// get the PTE of the faultaddress (walk current add space to that one ?) 
	// & check PTE_P ( to see if the page actually exists)

	(void)faulttype;
	(void)faultaddress;
	return 0;
}
