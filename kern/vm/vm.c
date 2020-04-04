
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
#include <machine/spl.h>
#include <machine/tlb.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <coremap.h>
#include <syscall.h>


/*
 * vm_bootstrap()
 * All the heavy lifting is done in coremap_bootstrap()
 */
void
vm_bootstrap(void)
{
	coremap_bootstrap();
	as_bitmap_bootstrap();
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
 * vm_fault()
 * Handles TLB faults. A TLB faults occur when hardware does not know how to translate
 * a virtual add since the translation is not present in any TLB entry. A load or store
 * cannot be completed if there isn't a TLB entry. As a result, we need to examine the fault
 * and determine whether to allocate a page to resolve the fault, or to kill the program.
 * 
 * Page faults imply TLB faults but not the other way around. Without load on demand or
 * swapping, there should never be any page faults... for now.
 * 
 * 1. Fault on Read, Page Fault: 		This is a SEG fault. Kill the program
 * 2. Fault on Read, No Page Fault: 	Add it to the TLB
 * 3. Fault on Write, Page Fault: 		If they are writing to the correct stackptr, then allocate a new page. Otherwise kill the program
 * 4. Fault on Write, No Page Fault: 	Add it into the TLB. Check if the page in question is writable though
 * 
 */ 
// Check if faultaddress is valid 
// User code or data segment; user heap between heap_start and heap_end; user stack

// For READ and WRITE
// get the PTE of the faultaddress (walk current add space to that one ?) 
// & check PTE_P ( to see if the page actually exists)

int
vm_fault(int faulttype, vaddr_t faultaddress)
{	
	/* Get current addrspace */
	struct addrspace *as = curthread->t_vmspace;
	assert(as != NULL);

	/* 
	 * Check whether faultaddress is a page fault or not 
	 * This will be need to be changed later when we implement swapping. 
	 * For now, if the entry exists in page table, it means it also exists in the coremap meaning no page fault
	 */
	int is_pagefault = 0;

	struct pte *entry = pt_get(as->as_pagetable, (faultaddress & PAGE_FRAME) );
	if(entry == NULL) {
		is_pagefault = 1;
	}
	
	/*
	 * Handle the faults
	 */
	switch(faulttype)
	{
		case VM_FAULT_READ:
			if(is_pagefault) {
				/* Read fault with page fault. We should kill the program */
				sys__exit(-1);
			}
			else {
				if( is_readable(entry->permissions) ) {
					/* No page fault and has read permission. Add to TLB */
					int idx;
					u_int32_t entryhi = (faultaddress & PAGE_FRAME);
					u_int32_t entrylo = entry->ppageaddr;
					idx = TLB_Replace(entryhi, entrylo);
					TLB_WriteDirty(idx, 0);
					TLB_WriteValid(idx, 1);
				}
				else {
					sys__exit(-1);
				}
			}
			break;

		case VM_FAULT_WRITE:
			if(is_pagefault) {
				sys__exit(-1);
			}
			else {
				if( is_writeable(entry->permissions) ) {
					/* No page fault and has write permission. Add to TLB */
					int idx;
					u_int32_t entryhi = (faultaddress & PAGE_FRAME);
					u_int32_t entrylo = entry->ppageaddr;
					idx = TLB_Replace(entryhi, entrylo);
					TLB_WriteDirty(idx, 1);
					TLB_WriteValid(idx, 1);
				}
				else {
					sys__exit(-1);
				}
			}
			break;

		case VM_FAULT_READONLY:
			/* This fault isn't useful right now. We will need it later tho when we do swapping */
			sys__exit(-1);
			break;

		default:
			panic("Unknown TLB fault!!!");
	}

	return 0;
}

