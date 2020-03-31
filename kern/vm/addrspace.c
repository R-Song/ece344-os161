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
	//as->array_regions = NULL;
	as->region_start = NULL;
	as->PTE_start = NULL;
	as->as_npages = 0; 
	as->as_pbase = 0;

	return as;
}

/*
 * Probably need to free some stuff in case struct arrays are not populated in the old addresspace 
 */ 
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

	// newas->array_regions = (struct region**)kmalloc(sizeof(old->array_regions) * sizeof(struct region*));
	// /* Calculate total npages from given regions of old as */
	// unsigned reg;
	// for(reg = 0; reg < sizeof(old->array_regions); reg++){
	// 	newas->array_regions[reg]->size = old->array_regions[reg]->size;
	// 	newas->array_regions[reg]->exec_new = old->array_regions[reg]->exec_new;
	// 	newas->array_regions[reg]->write_new = old->array_regions[reg]->write_new;
	// 	newas->array_regions[reg]->read_new = old->array_regions[reg]->read_new;
	// 	newas->array_regions[reg]->base_vaddr = old->array_regions[reg]->base_vaddr;
	// 	total_reg_size += old->array_regions[reg]->size;
	// }
	// /* This will NOT be needed IF we change as_create to take more args, not sure what they should be for now */
	// total_npages = (total_reg_size >> PAGE_OFFSET);
	// newas->as_npages = total_npages;
	
	struct region *region_cur = NULL;
	struct region *region_iter = old->region_start;

	region_cur = kmalloc(sizeof(struct region));
	if(region_cur == NULL){
		return ENOMEM;
	}
	if(region_iter == NULL){
		kfree(region_cur);
		region_cur = NULL;
	}

	newas->region_start = region_cur;

	while(region_iter != NULL){
		if(region_cur->next == NULL){
			region_cur->next = kmalloc(sizeof(struct region));
			if(region_cur->next == NULL){
				return ENOMEM;
			}
			region_cur->size = region_iter->size;
			total_reg_size += region_cur->size;
			region_cur->base_vaddr = region_iter->base_vaddr;

			region_cur->exec_new = region_iter->exec_new;
			region_cur->exec_old = region_iter->exec_old;

			region_cur->write_new = region_iter->write_new;
			region_cur->write_old = region_iter->write_old;

			region_cur->read_new = region_iter->read_new;
			region_cur->read_old = region_iter->read_old;

			region_cur = region_cur->next;
			region_cur->next = NULL;
			region_iter = region_iter->next;
		}
	} 

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
	struct region *curr_region = as->region_start;
	while(as->region_start->next != NULL){
		as->region_start = as->region_start->next;
		curr_region->next = NULL;
		kfree(curr_region);
		curr_region = NULL;
		curr_region = as->region_start;
	}
	curr_region = NULL;
	kfree(as->region_start);
	as->region_start = NULL;

	// unsigned i;
	// for (i = 0; i < sizeof(as->array_regions); i++){
	// 	kfree(as->array_regions[i]);
	// 	as->array_regions[i] = NULL;
	// }
	// kfree(as->array_regions);
	// as->array_regions = NULL;

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
	/* Dumbvm inspired first part */
	size_t npages; 
	int num_regions = 0; 	// I think there should be only two regions, code and data

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* Add a region to the as struct given the above specs */
	struct region *region_iter = as->region_start;
	while(region_iter != NULL){
		region_iter = region_iter->next;
		num_regions++;
	}
	region_iter = kmalloc(sizeof(struct region));
	num_regions++;
	/* I think there should be only two regions, code and data */
	if(num_regions > 2){
		return EUNIMP;
	}
	region_iter->size = sz;
	region_iter->base_vaddr = vaddr;
	region_iter->exec_new = writeable;
	region_iter->write_new = writeable;
	region_iter->read_new = readable;
	/* Not sure if the old flags should be set as the new ones here */
	region_iter->exec_old = executable;
	region_iter->write_old = writeable;
	region_iter->read_old = readable;
	region_iter->next = NULL;

	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/* Set all permissions to 1 and save the old permissions */

	// unsigned reg;
	// for(reg = 0; reg < sizeof(as->array_regions); reg++){
	// 	as->array_regions[reg]->exec_old = as->array_regions[reg]->exec_new;
	// 	as->array_regions[reg]->exec_new = 1;
	// 	as->array_regions[reg]->read_old = as->array_regions[reg]->read_new;
	// 	as->array_regions[reg]->read_new = 1;
	// 	as->array_regions[reg]->write_old = as->array_regions[reg]->write_new;
	// 	as->array_regions[reg]->write_new = 1;
	// }
	struct region *curr_region = as->region_start;
	while(curr_region!= NULL){
		curr_region->exec_old = curr_region->exec_new;
		curr_region->exec_new = 1;
		curr_region->read_old = curr_region->read_new;
		curr_region->read_new = 1;
		curr_region->write_old = curr_region->write_new;
		curr_region->write_new = 1;
		curr_region = curr_region->next;
	}
	return 0;
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
	struct region *curr_region = as->region_start;
	while(curr_region!= NULL){
		curr_region->exec_new = curr_region->exec_old;
		curr_region->read_new = curr_region->read_old;
		curr_region->write_new = curr_region->write_old;
		curr_region = curr_region->next;
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


