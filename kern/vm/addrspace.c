#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <coremap.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}
	as->heap_start = 0;
	as->heap_end = 0;
	as->array_regions = NULL;
	as->PTE_start = NULL;
	/*
	 * Initialize as needed.
	 */
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
	// set the flags according to the region of the address space
	int is_fixed = 0;
	int is_kernel = 0;
	int total_npages;
	int total_reg_size = 0;

	newas->array_regions = (struct region**)kmalloc(sizeof(old->array_regions) * sizeof(struct region*));
	/* Calculate total npages from given regions of old as */
	unsigned reg;
	for(reg = 0; reg < sizeof(old->array_regions); reg++){
		newas->array_regions[reg]->size = old->array_regions[reg]->size;
		newas->array_regions[reg]->exec_flag = old->array_regions[reg]->exec_flag;
		newas->array_regions[reg]->write_flag = old->array_regions[reg]->write_flag;
		newas->array_regions[reg]->read_flag = old->array_regions[reg]->read_flag;
		newas->array_regions[reg]->base_vaddr = old->array_regions[reg]->base_vaddr;
		total_reg_size += old->array_regions[reg]->size;
	}
	total_npages = (total_reg_size >> PAGE_OFFSET);
	paddr_t first_paddr = get_ppages(total_npages, is_fixed, is_kernel);
	paddr_t cur_paddr = first_paddr;
	
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
			PTE_cur = PTE_cur->next;
		}
		// same virtual address, but not same physical one
		PTE_cur->as_vpage = PTE_iter->as_vpage;
		PTE_cur->as_ppage = cur_paddr;
		PTE_cur->next = NULL;
		cur_paddr += PAGE_SIZE;
	}	

	newas->heap_start = old->heap_start;
	newas->heap_end = old->heap_end;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;  // suppress warning until code gets written
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
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
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


