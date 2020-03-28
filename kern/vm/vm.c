
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
#include <coremap.h>

/*
 * vm_bootstrap()
 * All the heavy lifting is done in coremap_bootstrap()
 */
void
vm_bootstrap(void)
{
	coremap_bootstrap();
	//coremap_mutex_bootstrap(); /* This is actually called in main... I'm not sure why it works there but not here*/
}


/*
 * alloc_kpages()
 * Allocate kernel pages. Kernel pages are direct mapped using the PADDR_TO_KVADDR macro
 * Read the comments in coremap.h for more information.
 */
vaddr_t 
alloc_kpages(int npages)
{	
	/* mark that we want the pages to be fixed and will be kernel pages */
	paddr_t paddr = get_ppages(npages, 1, 1);
	if(paddr == 0) {
		return 0;
	}
	return PADDR_TO_KVADDR(paddr);
}


/*
 * free_kpages()
 * Map the virtual address to physical address and free it from the coremap
 */
void 
free_kpages(vaddr_t addr)
{
	paddr_t paddr = addr - MIPS_KSEG0;
	free_ppages(paddr);
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

