
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
#include <bitmap.h>


/* current active address space id */
static asid_t curaddrspace;

/* flag to indicate whether or not to check we should be checking for specific asids */
static int curaddrspace_flag = 0;

/* debug flag that lets us enable tlb tags */
static int DEBUG_ASID_ENABLE = 0;

/* synchronization primitive for the asid bitmap */
static struct semaphore *as_bitmap_mutex = NULL;

/* 
 * Create the addrspace ASID bitmap 
 */
void
as_bitmap_bootstrap(void){
	as_bitmap =  bitmap_create((u_int32_t)NUM_ASID);	
	as_bitmap_mutex = sem_create("as bitmap mutex", 1);
	if(as_bitmap == NULL || as_bitmap_mutex == NULL) {
		panic("Address space bitmap could not be initialized");
	} 
}


/*
 * Initializes the datastructures for the addrspace
 * The regions as well as the actual page table
 */
struct addrspace *
as_create(void)
{
	int err;
	u_int32_t index;

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

	/* Attempt to get an available asid. If possible. */
	P(as_bitmap_mutex);
	err = bitmap_alloc(as_bitmap, &index);
	if(err){
		as->as_asid = NUM_ASID; /* Set to invalid asid just in case */
		as->as_asid_set = 0;	
	}
	else {
		as->as_asid = index;
		as->as_asid_set = 1;
	}
	V(as_bitmap_mutex);

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

	/* Give up the addrspace id */
	P(as_bitmap_mutex);
	if(as->as_asid_set) {
		bitmap_unmark(as_bitmap, as->as_asid);
	}
	V(as_bitmap_mutex);

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

	/* Set every single virtual address mapping to 0, this will change when we do copy on write */
	vaddr_t i = 0;
	while(1) {
		/* Should never be a page with vaddr_t 0 */
		i = pt_getnext(new->as_pagetable, i);
		if(i==0) {
			break; /* Been through all the pages */
		}
		assert(i < USERTOP); /* Should not ever go over user virtual address space */

		/* Allocate a physical page for this pte */
		struct pte *entry = pt_get(new->as_pagetable, i);
		entry->ppageaddr = 0;
	}

	/* 
	 * Allocate a new physical page for every virtual page in the page table
	 * This will be deprecated later when we do copy on write!
	 */
	while(1) {
		/* Should never be a page with vaddr_t 0 */
		i = pt_getnext(new->as_pagetable, i);
		if(i==0) {
			break; /* Been through all the pages */
		}
		assert(i < USERTOP); /* Should not ever go over user virtual address space */

		/* Get PTE from old as and new as */
		struct pte *old_entry = pt_get(old->as_pagetable, i);
		struct pte *new_entry = pt_get(new->as_pagetable, i);
		assert(new_entry->ppageaddr == 0);

		/* Allocate a page */
		new_entry->ppageaddr = get_ppages(1, 0, 0); /* ask for 1 non-fixed user-page */
		if(new_entry->ppageaddr == 0) {
			as_destroy(new);
			return ENOMEM;
		}

		/*
		* Copy contents of all pages
		* We do this by converting all physical pages to kernel addresses, then using memmove()
		*/
		memmove((void *)PADDR_TO_KVADDR(old_entry->ppageaddr), 
				(const void *)PADDR_TO_KVADDR(new_entry->ppageaddr),
				PAGE_SIZE);
	}

	*ret = new;
	return 0;
}


/*
 * as_prepare_load()
 * This function sets up the page table and allocates all the pages
 * This is called in load_elf()
 * 
 * This function will most likely be deprecated when we start doing load on demand
 */ 
int
as_prepare_load(struct addrspace *as)
{
	/* 
	 * We have to allocate memory for code and data segments
	 * Do this by adding entries into the page table
	 */
	unsigned i;
	struct pte *entry;
	vaddr_t addr;

	/* Code segment */
	for(i=0; i<as->as_code->npages; i++) {
		entry = (struct pte *)kmalloc(sizeof(struct pte));
		addr = (as->as_code->vbase + (i*PAGE_SIZE)); 
		pt_add(as->as_pagetable, addr, entry);
		/* Allocate a page for it */
		entry->ppageaddr = get_ppages(1, 0, 0); /* Ask for one non-fixed user level page */	   
		if(entry->ppageaddr == 0) {
			return ENOMEM;	/* Don't destroy addrspace because curthread->t_vmspace is destroyed in thread_exit */
		}
	}
	/* Data segment */
	for(i=0; i<as->as_data->npages; i++) {
		entry = (struct pte *)kmalloc(sizeof(struct pte));
		addr = (as->as_data->vbase + (i*PAGE_SIZE)); 
		pt_add(as->as_pagetable, addr, entry);
		/* Allocate a page for it */
		entry->ppageaddr = get_ppages(1, 0, 0);
		if(entry->ppageaddr == 0) {
			return ENOMEM;
		}
	}
	/* Heap and stack have 0 pages at first, so we don't need to allocate anything for them */

	/* Set permissions for the regions */
	as->as_code->exec_old = as->as_code->exec_new;
	as->as_code->write_old = as->as_code->write_new;
	as->as_code->read_old = as->as_code->read_new;

	as->as_code->exec_new = 1;
	as->as_code->write_new = 1;
	as->as_code->read_new = 1;

	as->as_data->exec_old = as->as_data->exec_new;
	as->as_data->write_old = as->as_data->write_new;
	as->as_data->read_old = as->as_data->read_new;

	as->as_data->exec_new = 1;
	as->as_data->write_new = 1;
	as->as_data->read_new = 1;	

	as->as_heap->exec_old = as->as_heap->exec_new;
	as->as_heap->write_old = as->as_heap->write_new;
	as->as_heap->read_old = as->as_heap->read_new;

	as->as_heap->exec_new = 1;
	as->as_heap->write_new = 1;
	as->as_heap->read_new = 1;

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
 * To be very careful, if we ever have to deal with addrspaces that were not allocated an ID we flush
 * Duplicate entries in the TLB is a fatal fault.
 * 
 * This is untested. If it doesn't work, just switch to the default of invalidating everything
 */
void
as_activate(struct addrspace *as)
{
	int i, spl;

	/* This area is critical as we are handling tlb as well as writing to curaddrspace globals */
	spl = splhigh();

	if(DEBUG_ASID_ENABLE) {
		/* Handle all 4 cases, curaddrspace_flag lets us know if the previous addrspace had an ID or not */
		if(curaddrspace_flag){
			if(as->as_asid_set) {
				curaddrspace = as->as_asid;
				curaddrspace_flag = 1;
				/* Invalidate all TLB entries by setting their valid bits to 0 */
				for(i=0; i<NUM_TLB; i++) {
					u_int32_t asid = TLB_ReadAsid(i);
					if(asid != curaddrspace) {
						TLB_WriteValid(i, 0); /* Set valid bit to 0 */
					}
					else {
						TLB_WriteValid(i, 1); /* Entry belongs to us, set valid bit to 1 */
					}
				}
			}
			else {
				curaddrspace = NUM_ASID; /* Set to invalid id just in case */
				curaddrspace_flag = 0;	 /* Current address space doesn't have an ID */
				/* Flush the TLB, can't risk duplicate entries in the TLB */
				TLB_Flush();
			}
		}
		else{
			if(as->as_asid_set) {
				curaddrspace = as->as_asid;
				curaddrspace_flag = 1;
				/* Even tho this addrspace has an ID, we can't risk duplicate entries from before, so flush */
				TLB_Flush();
			}
			else {
				curaddrspace = NUM_ASID; /* Set to invalid id just in case */
				curaddrspace_flag = 0;	 /* Current address space doesn't have an ID */
				/* Flush the TLB, can't risk duplicate entries in the TLB */
				TLB_Flush();
			}
		}
	}
	else {
		TLB_Flush();
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

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	if(as->as_code == 0){
		as->as_code->vbase = vaddr;
		as->as_code->npages = npages;
		as->as_code->exec_new = executable;
		as->as_code->write_new = writeable;
		as->as_code->read_new = readable;
		return 0;		
	}

	if(as->as_data == 0){
		as->as_data->vbase = vaddr;
		as->as_data->npages = npages;
		as->as_data->exec_new = executable;
		as->as_data->write_new = writeable;
		as->as_data->read_new = readable;
		return 0;		
	}

	return 0;
}


int
as_complete_load(struct addrspace *as)
{
	/* Reset all permissions back to their OG values as before load */
	as->as_code->exec_new = as->as_code->exec_old;
	as->as_code->write_new = as->as_code->write_old;
	as->as_code->read_new = as->as_code->read_old;

	as->as_data->exec_new = as->as_data->exec_old;
	as->as_data->write_new = as->as_data->write_old;
	as->as_data->read_new = as->as_data->read_old;

	as->as_heap->exec_new = as->as_heap->exec_old;
	as->as_heap->write_new = as->as_heap->write_old;
	as->as_heap->read_new = as->as_heap->read_old;

	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/* Initial user-level stack pointer */
	as->as_stackpbase = USERTOP;
	*stackptr = USERSTACK;
	
	return 0;
}

