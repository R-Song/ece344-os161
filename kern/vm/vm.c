
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
#include <synch.h>
#include <coremap.h>
#include <syscall.h>
#include <pagetable.h>
#include <permissions.h>
#include <vm_features.h>


struct lock *vm_fault_lock;

/*
 * vm_bootstrap()
 * All the heavy lifting is done in coremap_bootstrap()
 */
void
vm_bootstrap(void)
{
	coremap_bootstrap();
	as_bitmap_bootstrap();
	vm_fault_lock = lock_create("vm_fault_lock");
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
 * alloc_upages()
 * Allocate user pages. High level interface to pagetables and coremap.
 * Makes sure pagetables are consistent with coremap
 * Returns the new page table entry on success and NULL on failure
 */
struct pte *
alloc_upage(struct addrspace *as, vaddr_t vaddr)
{
	assert(vaddr%PAGE_SIZE == 0);
	pagetable_t pt = as->as_pagetable;

	/* Create a page table entry */
	struct pte *new_entry = pte_init();
	if(new_entry == NULL) {
		return NULL;
	}

	/* Get physical page from coremap */
	new_entry->ppageaddr = get_ppages(1, 0, 0);
	if(new_entry->ppageaddr == 0) {
		pte_destroy(new_entry);
		return NULL;
	}

	/* Add page table entry to the page table */
	pt_add(pt, vaddr, new_entry);

	return new_entry;
}

/*
 * free_upages()
 * Free user pages. 
 */
void
free_upage(struct addrspace *as, vaddr_t vaddr)
{
	assert(vaddr%PAGE_SIZE == 0);
	pagetable_t pt = as->as_pagetable;

	/* Get the page from the page table */
	struct pte* entry = pt_get(pt, vaddr);
	assert(entry != NULL);

	/* Free page from coremap */
	free_ppages(entry->ppageaddr);

	/* Remove the page from the pagetable */
	pt_remove(pt, vaddr);

	/* Deallocate the page table entry */
	pte_destroy(entry);
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

int
vm_fault(int faulttype, vaddr_t faultaddress)
{	
	int spl = splhigh();

	//kprintf("faultaddress at 0x%08x\n", faultaddress);

	int is_pagefault;
	int is_stack;

	vaddr_t faultpage;
	int retval;

	/* Get current addrspace */
	struct addrspace *as = curthread->t_vmspace;
	assert(as != NULL);

	/* Determine if fault address is a page fault or not */
	is_pagefault = 0;
	faultpage = (faultaddress & PAGE_FRAME);
	struct pte *faultentry = pt_get(as->as_pagetable, faultpage);
	if(faultentry == NULL) {
		is_pagefault = 1;
	}

	/* Check to see if faultpage is one below the current stackptr */
	is_stack = 0;
	if( faultpage == (as->as_stackptr - PAGE_SIZE) ) {
		is_stack = 1;
	}

	/* Check to see if address is valid */
	if( !is_vaddrcode(as, faultpage) && 
		!is_vaddrdata(as, faultpage) && 
		!is_vaddrheap(as, faultpage) && 
		!is_vaddrstack(as, faultpage) && 
		!is_stack ) {
			//kprintf("Page 0x%08x is not in a valid region\n", faultpage);
			return EFAULT;
		}

	/*
	 * Handle the faults
	 */
	switch(faulttype)
	{
		case VM_FAULT_READ:
			retval = vm_readfault(as, faultentry, faultaddress, is_pagefault, is_stack);
			break;

		case VM_FAULT_WRITE:
			retval = vm_writefault(as, faultentry, faultaddress, is_pagefault, is_stack);
			break;

		case VM_FAULT_READONLY:
			panic("Cannot handle vm_fault_readonly!!");

		default:
			retval = EINVAL;
	}

	splx(spl);
	return retval;
}

/* Need to check whether the page is already in memory or not (is_pagefault ?) 
 * If it IS, load appropriate entry into TLB and return from exception 
 * If it is NOT:
 * 		 - allocate a place in physical memory to store the page - use alloc_upage
 * 		 - load the page, using info from the program's ELF file to do so - call load_page_od
 * 		 - update the OS/161's information about this address space
 * 				* I suppose twe need an additional flag in  the PTE struct for whether or not the page is already in the mem
 *       - load appropriate entry into TLB and return from the exception - TLB_Replace + TLB_WriteDirty & Valid
 */

/* Handles faults on reads */
int vm_readfault(struct addrspace *as, struct pte *faultentry, vaddr_t faultaddress, int is_pagefault, int is_stack)
{
	lock_acquire(vm_fault_lock);
	assert(curspl>0);

	if(!LOAD_ON_DEMAND_ENABLE){
		(void) is_stack;
		(void) as;
	}

	int idx, result;
	vaddr_t faultpage = (faultaddress & PAGE_FRAME);
	off_t p_offset;

	if(is_pagefault) {
		if(LOAD_ON_DEMAND_ENABLE) {
			/* Check whether we are working with as_code or as_data segment */
			int is_code_seg = is_vaddrcode(as, faultpage);
			int is_data_seg = is_vaddrdata(as, faultpage);

			/* Sanity check - the only two other possibilities I see, since we already checked for validity, 
			 * is that the vaddr is in the heap or somewhere else on the stack
			 */
			if(!is_code_seg && !is_data_seg){
				lock_release(vm_fault_lock);
				return EFAULT;
			}

			struct pte *entry;
			/* Allocate space for it in physical memory to store it */
			entry = alloc_upage(as, faultpage);
			if(entry == NULL) {
				lock_release(vm_fault_lock);
				return ENOMEM;
			}
			entry->permissions = set_permissions(1, 1, 1); /* RWX */

			/* Add to the TLB */
			idx = TLB_Replace(faultpage, entry->ppageaddr);
			TLB_WriteDirty(idx, 1);
			TLB_WriteValid(idx, 1);	

			if(is_code_seg){
				p_offset = faultaddress - as->as_code->vbase;
				result = load_page_od(as->as_code->file, as->as_code->uio, p_offset);
				if(result){
					lock_release(vm_fault_lock);
					return result;
				}	
			}
			else if(is_data_seg){
				p_offset = faultaddress - as->as_data->vbase;
				result = load_page_od(as->as_data->file, as->as_data->uio, p_offset);
				if(result){
					lock_release(vm_fault_lock);
					return result;
				}			
			}

			lock_release(vm_fault_lock);
			return 0;	
		}
		else {
			//kprintf("Bad read at page 0x%x\n", faultpage);
			lock_release(vm_fault_lock);
			return EFAULT;
		}

	}
	else {
		if(is_readable(faultentry->permissions)) {
			idx = TLB_Replace(faultpage, faultentry->ppageaddr);
			/* Update dirty and valid bits in TLB entry */
			if(is_writeable(faultentry->permissions)) {
				TLB_WriteDirty(idx, 1);
			}
			TLB_WriteDirty(idx, 1);
			TLB_WriteValid(idx, 1);
			//TLB_Stat();
			lock_release(vm_fault_lock);
			return 0;
		}
		else {
			//kprintf("No permission to read at page 0x%x\n", faultpage);
			lock_release(vm_fault_lock);
			return EFAULT;
		}
	}
	lock_release(vm_fault_lock);
	panic("Should not reach here, vm_readfault");
	return 0;
}


/* Handles faults on writes */
int vm_writefault(struct addrspace *as, struct pte *faultentry, vaddr_t faultaddress, int is_pagefault, int is_stack)
{
	lock_acquire(vm_fault_lock);
	assert(curspl>0);
	int idx;
	int result;

	vaddr_t faultpage = (faultaddress & PAGE_FRAME);
	off_t p_offset;

	/* Check to see if page fault is to the stack. If so we should allocate a page for the stack. */
	if(is_pagefault && is_stack) {
		struct pte *new_stack_entry = alloc_upage(as, faultpage);
		if(new_stack_entry == NULL) {
			//kprintf("Out of memory while creating stack");
			lock_release(vm_fault_lock);
			return ENOMEM;
		}
		
		as->as_stackptr -= PAGE_SIZE;
		assert(faultpage == as->as_stackptr);
		new_stack_entry->permissions = set_permissions(1, 1, 0);

		/* Add to the TLB */
		idx = TLB_Replace(faultpage, new_stack_entry->ppageaddr);
		TLB_WriteDirty(idx, 1);
		TLB_WriteValid(idx, 1);
		//TLB_Stat();
		lock_release(vm_fault_lock);
		return 0;
	}

	if(is_pagefault && !is_stack) {
		if(LOAD_ON_DEMAND_ENABLE) {
			/* Check whether we are working with as_code or as_data segment */
			int is_code_seg = is_vaddrcode(as, faultpage);
			int is_data_seg = is_vaddrdata(as, faultpage);

			/* Sanity check - the only two other possibilities I see, since we already checked for validity, 
			 * is that the vaddr is in the heap or somewhere else on the stack
			 */
			if(!is_code_seg && !is_data_seg){
				lock_release(vm_fault_lock);
				return EFAULT;
			}
			struct pte *entry;
			/* Allocate space for it in physical memory to store it */
			entry = alloc_upage(as, faultpage);
			if(entry == NULL) {
				lock_release(vm_fault_lock);
				return ENOMEM;
			}
			entry->permissions = set_permissions(1, 1, 1); /* RWX */

			/* Add to the TLB */
			idx = TLB_Replace(faultpage, entry->ppageaddr);
			TLB_WriteDirty(idx, 1);
			TLB_WriteValid(idx, 1);

			if(is_code_seg){
				p_offset = faultaddress - as->as_code->vbase;
				result = load_page_od(as->as_code->file, as->as_code->uio, p_offset);
				if(result){
					lock_release(vm_fault_lock);
					return result;
				}	
			}
			else if(is_data_seg){
				p_offset = faultaddress - as->as_data->vbase;
				result = load_page_od(as->as_data->file, as->as_data->uio, p_offset);
				if(result){
					lock_release(vm_fault_lock);
					return result;
				}			
			}

			lock_release(vm_fault_lock);
			return 0;	
		}
		else {
			/* Illegal write to unallocated memory to the stack. Segfault. */
		    //kprintf("Bad write to page 0x%x\n", faultpage);
			lock_release(vm_fault_lock);
		    return EFAULT;
		}
	}

	/* If not page fault, check permissions then add to the TLB */
	if(!is_pagefault) {
		if( is_writeable(faultentry->permissions) ) {
			idx = TLB_Replace(faultpage, faultentry->ppageaddr);
			TLB_WriteDirty(idx, 1);
			TLB_WriteValid(idx, 1);
			lock_release(vm_fault_lock);
			return 0;
		}
		else {
			//kprintf("No permission to write to page 0x%x\n", faultpage);
			lock_release(vm_fault_lock);
			return EFAULT;
		}
	}
	lock_release(vm_fault_lock);
	panic("Should not reach here, vm_writefault");
	return 0;
}

