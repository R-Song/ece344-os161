#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <coremap.h>
#include <machine/tlb.h>
#include <synch.h>
#include <machine/spl.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	/*
	 * Not sure if we are to take in more args and set more precise parameters
	 */
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}
	as->heap_start = 0;
	as->heap_end = 0;
	as->array_regions = NULL;
	as->PTE_start = NULL;
	as->as_npages = 0; 
	as->as_pbase = 0;

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}
	/* set the flags according to the region of the address space */
	int is_fixed = 0;
	int is_kernel = 0;
	int total_npages;
	int total_reg_size = 0;

	newas->array_regions = (struct region**)kmalloc(sizeof(old->array_regions) * sizeof(struct region*));
	/* Calculate total npages from given regions of old as */
	unsigned reg;
	for(reg = 0; reg < sizeof(old->array_regions); reg++){
		newas->array_regions[reg]->size = old->array_regions[reg]->size;
		newas->array_regions[reg]->exec_new = old->array_regions[reg]->exec_new;
		newas->array_regions[reg]->write_new = old->array_regions[reg]->write_new;
		newas->array_regions[reg]->read_new = old->array_regions[reg]->read_new;
		newas->array_regions[reg]->base_vaddr = old->array_regions[reg]->base_vaddr;
		total_reg_size += old->array_regions[reg]->size;
	}
	/* This will NOT be needed IF we change as_create to take more args, not sure what they should be for now */
	total_npages = (total_reg_size >> PAGE_OFFSET);
	newas->as_npages = total_npages;
	/* Get the first physical address*/
	paddr_t first_paddr = get_ppages(total_npages, is_fixed, is_kernel);
	if(first_paddr == 0){
		return ENOMEM;
	}
	paddr_t cur_paddr = first_paddr;
	/**/
	struct PTE *PTE_cur = NULL;   
	struct PTE *PTE_iter = old->PTE_start;

	PTE_cur = kmalloc(sizeof(struct PTE));
	if(PTE_cur == NULL){
		return ENOMEM;
	}
	newas->PTE_start = PTE_cur;

	while(PTE_iter != NULL){
		if(PTE_cur->next == NULL){        //should use ASSERT instead??
			PTE_cur->next = kmalloc(sizeof(struct PTE));
			if(PTE_cur->next == NULL){
				return ENOMEM;
			}
			PTE_cur->as_vpage = PTE_iter->as_vpage;
			PTE_cur->as_ppage = cur_paddr;
			PTE_cur = PTE_cur->next;
			PTE_cur->next = NULL;
			/* Copy old page from memory into the new page */
			memmove((void *)PADDR_TO_KVADDR(PTE_cur->as_ppage), 
				(const void *)PADDR_TO_KVADDR(PTE_iter->as_ppage),
				PAGE_SIZE);

			cur_paddr += PAGE_SIZE;
			PTE_iter = PTE_iter->next;
		}
	}	

	newas->heap_start = old->heap_start;
	newas->heap_end = old->heap_end;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/* Clean up Page Table */
	struct PTE *curr_PTE = as->PTE_start;
	while(as->PTE_start->next != NULL){
		as->PTE_start = as->PTE_start->next;
		curr_PTE->next = NULL;
		kfree(curr_PTE);
		curr_PTE = NULL;
		curr_PTE = as->PTE_start;
	}
	/* Remove last non-NULL PTE */
	curr_PTE = NULL;
	kfree(as->PTE_start);
	as->PTE_start = NULL;
	/* Free the regions array */
	unsigned i;
	for (i = 0; i < sizeof(as->array_regions); i++){
		kfree(as->array_regions[i]);
		as->array_regions[i] = NULL;
	}
	kfree(as->array_regions);
	as->array_regions = NULL;

	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	(void)as;

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
	/*
	 * Write this.
	 */

	(void)as;
	(void)vaddr;
	(void)sz;
	(void)readable;
	(void)writeable;
	(void)executable;
	return EUNIMP;
}

int
as_prepare_load(struct addrspace *as)
{
	//if (as==NULL) {
	//	return -1;
	//}
	/* Set all permissions to 1 and save the old permissions */
	unsigned reg;
	for(reg = 0; reg < sizeof(as->array_regions); reg++){
		as->array_regions[reg]->exec_old = as->array_regions[reg]->exec_new;
		as->array_regions[reg]->exec_new = 1;
		as->array_regions[reg]->read_old = as->array_regions[reg]->read_new;
		as->array_regions[reg]->read_new = 1;
		as->array_regions[reg]->write_old = as->array_regions[reg]->write_new;
		as->array_regions[reg]->write_new = 1;
	}
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/* Reset all permissions back to their OG values as before load */
	unsigned reg;
	for(reg = 0; reg < sizeof(as->array_regions); reg++){
		as->array_regions[reg]->exec_new = as->array_regions[reg]->exec_old;
		as->array_regions[reg]->read_new = as->array_regions[reg]->read_old;
		as->array_regions[reg]->write_new = as->array_regions[reg]->write_old;
	}
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


