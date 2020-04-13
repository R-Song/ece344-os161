
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
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <coremap.h>
#include <swap.h>
#include <syscall.h>
#include <pagetable.h>
#include <permissions.h>
#include <vm_features.h>


/* Synchronization device for vm_fault. Read swap.h for the reason behind it */
static struct lock *fault_lock;

/*
 * vm_bootstrap()
 * All the heavy lifting is done in coremap_bootstrap()
 */
void
vm_bootstrap(void)
{
	coremap_bootstrap();
	as_bitmap_bootstrap();
	fault_lock = lock_create("vm fault lock");
	if(fault_lock == NULL) {
		panic("Could not create fault lock...");
	}
}


/*
 * alloc_kpages()
 * Allocate kernel pages. Kernel pages are direct mapped using the PADDR_TO_KVADDR macro
 * Read the comments in coremap.h for more information.
 */
vaddr_t 
alloc_kpages(int npages)
{	
	int spl = splhigh();
	paddr_t paddr;
	int err;

	/* mark that we want the pages to be fixed and will be kernel pages */
	paddr = get_ppages(npages, 1, NULL);
	if(paddr != 0) {
		splx(spl);
		return PADDR_TO_KVADDR(paddr);
	}
	
if(SWAPPING_ENABLE) {
	int lock_held_prior = lock_do_i_hold(swap_lock);
	lock_acquire(swap_lock);

	err = swap_createspace(npages);
	if(err) {
		if(!lock_held_prior) {
			lock_release(swap_lock);
		}
		splx(spl);
		return 0;
	}
	/* try again */
	paddr = get_ppages(npages, 1, NULL);
	if(paddr == 0) {
		panic("Something seriously wrong with swap");
	}
	if(!lock_held_prior) {
		lock_release(swap_lock);
	}
	splx(spl);
	return PADDR_TO_KVADDR(paddr);
}

	splx(spl);
	return 0;
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
void
alloc_upage(struct pte *entry)
{
	int err;
	int spl = splhigh();

	assert(entry != NULL);
	assert(entry->ppageaddr == 0);
	assert( lock_do_i_hold(swap_lock) );

	/* Get physical page from coremap */
	entry->ppageaddr = get_ppages(1, 0, entry);
	if(entry->ppageaddr != 0) {
		splx(spl);
		return;
	}

if(SWAPPING_ENABLE) {
	err = swap_pageout();
	if(err) {
		splx(spl);
		return;
	}
	entry->ppageaddr = get_ppages(1, 0, entry);
	if(entry->ppageaddr == 0) {
		panic("Something messed up with swap");
	}
}
	splx(spl);
}

/*
 * free_upages()
 * Free user pages depending on its swap state 
 */
void
free_upage(struct pte *entry)
{
	int spl = splhigh();
	assert(lock_do_i_hold(swap_lock));

	/* Depending on the swap state, we free differently */
	switch(entry->swap_state) {
		case PTE_PRESENT:
			assert(entry->ppageaddr != 0);
			free_ppages(entry->ppageaddr);
			entry->ppageaddr = 0;
			entry->swap_state = PTE_NONE;
			break;
		case PTE_SWAPPED:
			swap_freepage(entry->swap_location);
			entry->ppageaddr = 0;
			entry->swap_state = PTE_NONE;
			break;
		case PTE_DIRTY:
		case PTE_CLEAN:
			assert(entry->ppageaddr != 0);
			free_ppages(entry->ppageaddr);
			swap_freepage(entry->swap_location);
			entry->ppageaddr = 0;
			entry->swap_state = PTE_NONE;
			break;
		default:
			panic("invalid swap state!!\n");
	}

	splx(spl);
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
	lock_acquire(fault_lock);

	//kprintf("faultaddress at 0x%08x\n", faultaddress);

	int is_pagefault;
	int is_stack;
	int is_swapped;
	vaddr_t faultpage;
	int retval;

	/* Get current addrspace */
	struct addrspace *as = curthread->t_vmspace;
	assert(as != NULL);

	/* Detetermine a number of flags: is_pagefault, is_swapped, is_stack */
	is_pagefault = 0;
	faultpage = (faultaddress & PAGE_FRAME);
	struct pte *faultentry = pt_get(as->as_pagetable, faultpage);
	if(faultentry == NULL) {
		is_pagefault = 1;
	}

	is_swapped = 0;
	if(faultentry != NULL) {
		if(faultentry->swap_state == PTE_SWAPPED) {
			is_swapped = 1;
		}
	}

	is_stack = 0;
	if( faultpage >= (USERSTACKBASE) ) {
		is_stack = 1;
	}

	/* Check to see if address is valid */
	if( !is_vaddrcode(as, faultpage) && 
		!is_vaddrdata(as, faultpage) && 
		!is_vaddrheap(as, faultpage) && 
		!is_vaddrstack(as, faultpage) && 
		!is_stack ) {
			//kprintf("Page 0x%08x is not in a valid region\n", faultpage);
			lock_release(fault_lock);
			splx(spl);
			return EFAULT;
		}

	/*
	 * Handle the faults
	 */
	switch(faulttype)
	{
		case VM_FAULT_READ:
			retval = vm_readfault(as, faultentry, faultpage, is_pagefault, is_stack, is_swapped);
			break;

		case VM_FAULT_WRITE:
			retval = vm_writefault(as, faultentry, faultaddress, is_pagefault, is_stack, is_swapped);
			break;

		case VM_FAULT_READONLY:
			panic("Cannot handle vm_fault_readonly!!");

		default:
			retval = EINVAL;
	}

	lock_release(fault_lock);
	splx(spl);
	return retval;
}

/* Need to check whether the page is already in memory or not 
 * If it IS, load appropriate entry into TLB and return from exception
 * If it is NOT:
 * 		 - allocate a place in physical memory to store the page - use alloc_upage
 * 		 - load the page, using info from the program's ELF file to do so - call load_page_od
 * 		 - update the OS/161's information about this address space
 * 				* I suppose twe need an additional flag in  the PTE struct for whether or not the page is already in the mem
 *       - load appropriate entry into TLB and return from the exception - TLB_Replace + TLB_WriteDirty & Valid
 */

/* Handles faults on reads */
int vm_readfault(struct addrspace *as, struct pte *faultentry, vaddr_t faultaddress, int is_pagefault, int is_stack, int is_swapped)
{
	assert(curspl>0);
	(void) is_stack;
	(void) as;
	int idx;
	//int err;

	if(!LOAD_ON_DEMAND_ENABLE){
		(void) is_stack;
		(void) as;
	}

	//int result;
	vaddr_t faultpage = (faultaddress & PAGE_FRAME);
	//off_t p_offset;

	if(is_pagefault) {
		// if(LOAD_ON_DEMAND_ENABLE) {
		// 	/* Check whether we are working with as_code or as_data segment */
		// 	int is_code_seg = is_vaddrcode(as, faultpage);
		// 	int is_data_seg = is_vaddrdata(as, faultpage);

		// 	/* Sanity check - the only two other possibilities I see, since we already checked for validity, 
		// 	 * is that the vaddr is in the heap or somewhere else on the stack
		// 	 */
		// 	if(!is_code_seg && !is_data_seg){
		// 		return EFAULT;
		// 	}

		// 	struct pte *entry;
		// 	/* Allocate space for it in physical memory to store it */
		// 	entry = alloc_upage(as, faultpage);
		// 	if(entry == NULL) {
		// 		return ENOMEM;
		// 	}
		// 	entry->permissions = set_permissions(1, 1, 1); /* RWX */

		// 	if(is_code_seg){
		// 		p_offset = faultaddress - as->as_code->vbase;
		// 		result = load_page_od(as->as_code->file, as->as_code->uio, p_offset);
		// 		if(result){
		// 			return result;
		// 		}	
		// 	}
		// 	else if(is_data_seg){
		// 		p_offset = faultaddress - as->as_data->vbase;
		// 		result = load_page_od(as->as_data->file, as->as_data->uio, p_offset);
		// 		if(result){
		// 			return result;
		// 		}			
		// 	}
		// 	/* Add to the TLB */
		//     idx = TLB_Replace(faultpage, entry->ppageaddr);
		// 	TLB_WriteDirty(idx, 1);
		// 	TLB_WriteValid(idx, 1);	
		// 	//TLB_Stat();
		// 	return 0;	
		// }
		//else {
			//kprintf("Bad read at page 0x%x\n", faultpage);
			return EFAULT;
		//}

	}
	else if(!is_pagefault && !is_swapped) {
		if(is_readable(faultentry->permissions)) {
			idx = TLB_Replace(faultpage, faultentry->ppageaddr);
			if(is_writeable(faultentry->permissions)) {
				TLB_WriteDirty(idx, 1);
			}
			TLB_WriteValid(idx, 1);
			return 0;
		}
		else {
			kprintf("No permission to read at page 0x%x\n", faultpage);
			return EFAULT;
		}
	}
	else if(!is_pagefault && is_swapped) {
		return vm_swapfault(faultentry, faultaddress, VM_FAULT_READ);
	}

	panic("Should not reach here, vm_readfault");
	return 0;
}


/* Handles faults on writes */
int vm_writefault(struct addrspace *as, struct pte *faultentry, vaddr_t faultaddress, int is_pagefault, int is_stack, int is_swapped)
{
	assert(curspl>0);

	int idx;
	//int err;
	//int result;

	vaddr_t faultpage = (faultaddress & PAGE_FRAME);
	//off_t p_offset;

	/* Check to see if page fault is to the stack. If so we should allocate a page for the stack. */
	if(is_pagefault && is_stack) {
		return vm_stackfault(as, faultaddress);
	}

	if(is_pagefault && !is_stack) {
		// if(LOAD_ON_DEMAND_ENABLE) {
		// 	/* Check whether we are working with as_code or as_data segment */
		// 	int is_code_seg = is_vaddrcode(as, faultpage);
		// 	int is_data_seg = is_vaddrdata(as, faultpage);

		// 	/* Sanity check - the only two other possibilities I see, since we already checked for validity, 
		// 	 * is that the vaddr is in the heap or somewhere else on the stack
		// 	 */
		// 	if(!is_code_seg && !is_data_seg){
		// 		return EFAULT;
		// 	}
		// 	struct pte *entry;
		// 	/* Allocate space for it in physical memory to store it */
		// 	entry = alloc_upage(as, faultpage);
		// 	if(entry == NULL) {
		// 		return ENOMEM;
		// 	}
		// 	entry->permissions = set_permissions(1, 1, 1); /* RWX */

		// 	if(is_code_seg){
		// 		p_offset = faultaddress - as->as_code->vbase;
		// 		result = load_page_od(as->as_code->file, as->as_code->uio, p_offset);
		// 		if(result){
		// 			return result;
		// 		}	
		// 	}
		// 	else if(is_data_seg){
		// 		p_offset = faultaddress - as->as_data->vbase;
		// 		result = load_page_od(as->as_data->file, as->as_data->uio, p_offset);
		// 		if(result){
		// 			return result;
		// 		}			
		// 	}
		// 	/* Add to the TLB */
		//     idx = TLB_Replace(faultpage, entry->ppageaddr);
		// 	TLB_WriteDirty(idx, 1);
		// 	TLB_WriteValid(idx, 1);	
		// 	//TLB_Stat();
		// 	return 0;	
		// }
		//else {
			/* Illegal write to unallocated memory to the stack. Segfault. */
		    //kprintf("Bad write to page 0x%x\n", faultpage);
		    return EFAULT;
		//}
	}

	/* If not page fault, check permissions then add to the TLB */
	if(!is_pagefault && !is_swapped) {
		if( is_writeable(faultentry->permissions) ) {
			idx = TLB_Replace(faultpage, faultentry->ppageaddr);
			TLB_WriteDirty(idx, 1);
			TLB_WriteValid(idx, 1);
			return 0;
		}
		else {
			kprintf("No permission to write to page 0x%x\n", faultpage);
			return EFAULT;
		}
	}
	else if(!is_pagefault && is_swapped) {
		return vm_swapfault(faultentry, faultaddress, VM_FAULT_WRITE);
	}
	else {
		kprintf("No permission to write to page 0x%x\n", faultpage);
		return EFAULT;
	}
	panic("Should not reach here, vm_writefault");
	return 0;
}

/* Handle a fault that results from writing to the stack */
int vm_stackfault(struct addrspace *as, vaddr_t faultaddress) 
{
		int idx, err;
		paddr_t faultpage_paddr;
		struct pte *new_stack_entry;
		vaddr_t vpageaddr;
		vaddr_t faultpage = (faultaddress & PAGE_FRAME);
		int num_requested_pages = ((as->as_stackptr - faultpage) >> PAGE_OFFSET);

		for(idx=0; idx<num_requested_pages; idx++) {
			lock_acquire(swap_lock);

			vpageaddr = faultpage + PAGE_SIZE*idx;

			new_stack_entry = pte_init();
			if(new_stack_entry == NULL) {
				return ENOMEM;
			}

			if(idx==0) {
				/* For the fault page, we actually give it a physical page */
				alloc_upage(new_stack_entry);
				if(new_stack_entry->ppageaddr == 0) {
					lock_release(swap_lock);
					pte_destroy(new_stack_entry);
					return ENOMEM;
				}
				faultpage_paddr = new_stack_entry->ppageaddr;
				
				/* entry->ppageaddr is updated by alloc_upage */
				new_stack_entry->permissions = set_permissions(1, 1, 0);
				new_stack_entry->swap_state = PTE_PRESENT;
				new_stack_entry->swap_location = 0;
			}
			else {
				/* for the others, we simply allocate swap storage */
				err = swap_allocpage_od(new_stack_entry);
				if(err) {
					lock_release(swap_lock);
					return err;
				}
				new_stack_entry->permissions = set_permissions(1, 1, 0);
			}

			/* add entry to page table */
			pt_add(as->as_pagetable, vpageaddr, new_stack_entry);

			lock_release(swap_lock);
		}	

		as->as_stackptr -= PAGE_SIZE*num_requested_pages;
		assert(as->as_stackptr == faultpage);

		/* Add to the TLB */
		idx = TLB_Replace(faultpage, faultpage_paddr);
		TLB_WriteDirty(idx, 1);
		TLB_WriteValid(idx, 1);
		return 0;
}


/* Handle a fault that results from reading/writing to a swapped page */
int vm_swapfault(struct pte *faultentry, vaddr_t faultaddress, int faulttype)
{
	int err, idx;
	vaddr_t faultpage = (faultaddress & PAGE_FRAME);

	/* Check to see if we have proper permissions */
	if(faulttype == VM_FAULT_READ && !is_readable(faultentry->permissions)) {
		return EFAULT;
	}
	else if(faulttype == VM_FAULT_WRITE && !is_writeable(faultentry->permissions)) {
		return EFAULT;
	}

	lock_acquire(swap_lock);

	/* Lets swap in the page */
	err = swap_pagein(faultentry);
	if(err) {
		return err;
	}
	assert(faultentry->swap_state == PTE_CLEAN ); /* Just loaded the page, it should be clean */
	assert(faultentry->ppageaddr != 0);

	faultentry->swap_state = PTE_DIRTY; 	/* For now we assume that its just dirty. Later we will use dirty bit and readonly fault. */

	/* update TLB */
	idx = TLB_Replace(faultpage, faultentry->ppageaddr);
	TLB_WriteDirty(idx, 1);
	TLB_WriteValid(idx, 1);

	lock_release(swap_lock);
	return 0;
}


