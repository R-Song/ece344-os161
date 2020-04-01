
/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <coremap.h>
#include <machine/tlb.h>
#include <synch.h>
#include <machine/spl.h>
#include <pagetable.h>


/* current active address space */
static asid_t curaddrspace;


/*
 * Initializes the datastructures for the addrspace
 * The regions as well as the actual page table
 */
struct addrspace *
as_create(void)
{
	/* A little bit redundant but the readability is worth it I think */
	struct addrspace *as = (struct addrspace *)kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->as_pagetable = pt_init();
	if(as->as_pagetable == NULL) {
		kfree(as);
		return NULL;
	}

	as->as_code = (struct as_region *)kmalloc(sizeof(struct as_region));
	if(as->as_code == NULL) {
		pt_destroy(as->as_pagetable);
		kfree(as);
		return NULL;
	}

	as->as_data = (struct as_region *)kmalloc(sizeof(struct as_region));
	if(as->as_data == NULL) {
		kfree(as->as_code);
		pt_destroy(as->as_pagetable);
		kfree(as);
		return NULL;
	}

	as->as_heap = (struct as_region *)kmalloc(sizeof(struct as_region));
	if(as->as_heap == NULL) {
		kfree(as->as_data);
		kfree(as->as_code);
		pt_destroy(as->as_pagetable);
		kfree(as);
		return NULL;
	}

	/* Initialize everything */
	as->as_code->vbase = 0;
	as->as_code->npages = 0;
	as->as_data->vbase = 0;
	as->as_data->npages = 0;
	as->as_heap->vbase = 0;
	as->as_heap->npages = 0;
	as->as_stackpbase = 0;

	return as;
}


/*
 * Destroy address space
 * pt_destroy() handles destroying all the pte's
 */
void
as_destroy(struct addrspace *as)
{
	assert(as != NULL);
	/* Destroy the regions, pagetable, and as */
	kfree(as->as_code);
	kfree(as->as_data);
	kfree(as->as_heap);
	pt_destroy(as->as_pagetable);
	kfree(as);
}


/*
 * as_copy()
 * 
 * Creates an identical copy of an address space.
 * Currently the implementation does not support copy on write. Therefore new pages are given to 
 */ 
int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	int err;
	/* Allocate space for new addrspace */
	struct addrspace *new;
	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	/* Copy over virtual address fields */
	new->as_code->vbase = old->as_code->vbase;
	new->as_code->npages = old->as_code->npages;
	new->as_data->vbase = old->as_data->vbase;
	new->as_data->npages = old->as_data->npages;
	new->as_heap->vbase = old->as_heap->vbase;
	new->as_heap->npages = old->as_heap->npages;

	/* should we copy the flags?????? */

	/* 
	 * Copy entries to the page table
	 * This is a deep copy, including all the pte entries. So the mappings for each page table
	 * are identical. This can be used to later implement copy on write
	 */
	err = pt_copy(old->as_pagetable, new->as_pagetable);
	if(err) {
		as_destroy(new);
		return ENOMEM;
	}

	/* 
	 * Call as_prepare_load()
	 * This will do the copying of the page table as well as allocation of new pages 
	 * This will be deprecated later when we do copy on write and load on demand
	 * Later to support copy on write, we will probably have to have a dirty field in the page table
	 */
	err = as_prepare_load(new);
	if(err) {
		as_destroy(new);
		return ENOMEM;
	}

	/*
	 * Copy contents of all pages
	 * We do this by converting all physical pages to kernel addresses, then using memmove()
	 * This part is a little confusing, but basically just finding each populated page in the page table.
	 */
	vaddr_t i = 0;
	while(1) {
		/* Should never be a page with vaddr_t 0 */
		i = pt_getnext(new->as_pagetable, i);
		if(i==0) {
			break; /* Been through all the pages */
		}
		assert(i < USERTOP); /* Should not ever increment over user virtual address space */

		struct pte *old_entry = pt_get(old->as_pagetable, i);
		struct pte *new_entry = pt_get(new->as_pagetable, i);
		memmove((void *)PADDR_TO_KVADDR(old_entry->ppage), 
				(const void *)PADDR_TO_KVADDR(new_entry->ppage),
				PAGE_SIZE);
	}

	*ret = new;
	return 0;
}


/*
 * as_prepare_load()
 * This function does the allocates pages for each of the virtual addresses in the page table
 * This function will most likely be deprecated when we start doing load on demand
 */ 
int
as_prepare_load(struct addrspace *as)
{
	/* 
	 * Do page allocations. Loop through all existing pages in the 
	 * page table and allocate a new phyiscal page for all of them.
	 */
	vaddr_t i = 0;
	while(1) {
		/* Should never be a page with vaddr_t 0 */
		i = pt_getnext(as->as_pagetable, i);
		if(i==0) {
			break; /* Been through all the pages */
		}
		assert(i < USERTOP); /* Should not ever increment over user virtual address space */

		/* Allocate a physical page for this pte */
		struct pte *entry = pt_get(as->as_pagetable, i);
		entry->ppage = get_ppages(1, 0, 0); /* ask for 1 non-fixed user-page */
		if(entry->ppage == 0) {
			return ENOMEM;
		}
	}

	/* Set permissions for the regions */


	
	return 0;
}


/*
 * as_activate()
 * 
 * DUMBVM does this by flushing the entire TLB. This actually makes the context switch slow.
 * A better way is to not flush the TLB, but have address space ID's that identify each address space 
 * accompanied by a global variable that lets the kernel know what is the current address space, and what
 * TLB entries to look at upon a fault.
 * 
 * For now we will just invalidate entries, but later we should implement address space ID's.
 */
void
as_activate(struct addrspace *as)
{
	(void) as;
	(void) curaddrspace;

	int spl;
	int i;

	spl = splhigh();
	
	for(i = 0; i < NUM_TLB; i++){
		TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}


/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	(void) as;
	(void) vaddr;
	(void) sz;
	(void) readable;
	(void) writeable;
	(void) executable;
	return 0;
	// /* Dumbvm inspired first part */
	// size_t npages; 
	// int num_regions = 0; 	// I think there should be only two regions, code and data

	// /* Align the region. First, the base... */
	// sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	// vaddr &= PAGE_FRAME;

	// /* ...and now the length. */
	// sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	// npages = sz / PAGE_SIZE;

	// /* Add a region to the as struct given the above specs */
	// struct region *region_iter = as->region_start;
	// while(region_iter != NULL){
	// 	region_iter = region_iter->next;
	// 	num_regions++;
	// }
	// region_iter = kmalloc(sizeof(struct region));
	// num_regions++;
	// /* I think there should be only two regions, code and data */
	// if(num_regions > 2){
	// 	return EUNIMP;
	// }
	// region_iter->size = sz;
	// region_iter->base_vaddr = vaddr;
	// region_iter->exec_new = writeable;
	// region_iter->write_new = writeable;
	// region_iter->read_new = readable;
	// /* Not sure if the old flags should be set as the new ones here */
	// region_iter->exec_old = executable;
	// region_iter->write_old = writeable;
	// region_iter->read_old = readable;
	// region_iter->next = NULL;

	// return 0;
}



int
as_complete_load(struct addrspace *as)
{
	/* Reset all permissions back to their OG values as before load */

	// unsigned reg;
	// for(reg = 0; reg < sizeof(as->array_regions); reg++){
	// 	as->array_regions[reg]->exec_new = as->array_regions[reg]->exec_old;
	// 	as->array_regions[reg]->read_new = as->array_regions[reg]->read_old;
	// 	as->array_regions[reg]->write_new = as->array_regions[reg]->write_old;
	// }
	// struct region *curr_region = as->region_start;
	// while(curr_region!= NULL){
	// 	curr_region->exec_new = curr_region->exec_old;
	// 	curr_region->read_new = curr_region->read_old;
	// 	curr_region->write_new = curr_region->write_old;
	// 	curr_region = curr_region->next;
	// }
	(void) as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	
	return 0;
}

// look up page table 
// int
// as_fault()


