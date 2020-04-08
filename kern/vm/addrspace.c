
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
#include <permissions.h>


/* current active address space id */
static asid_t curaddrspace;

/* flag to indicate whether or not to check we should be checking for specific asids */
static int curaddrspace_flag = 0;

/* bitmap for maintaining ASIDs */
static struct bitmap *as_bitmap;

/* synchronization primitive for the asid bitmap */
static struct semaphore *as_bitmap_mutex = NULL;



/* debug flag that enables tlb tags */
static int DEBUG_ASID_ENABLE = 0;



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

	/* Attempt to get an available asid. If possible. */
	if(DEBUG_ASID_ENABLE) {
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
	}
	else {
		as->as_asid = NUM_ASID; /* Set to invalid asid just in case */
		as->as_asid_set = 0;	
	}


	/* Initialize everything */
	as->as_code->vbase = 0;
	as->as_code->npages = 0;
	as->as_code->permissions = set_permissions(0, 0, 0);

	as->as_data->vbase = 0;
	as->as_data->npages = 0;
	as->as_data->permissions = set_permissions(0, 0, 0);

	as->as_heapstart = 0;
	as->as_heapend = 0;
	as->as_stackptr = 0;

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
	if(DEBUG_ASID_ENABLE) {
		P(as_bitmap_mutex);
		if(as->as_asid_set) {
			bitmap_unmark(as_bitmap, as->as_asid);
		}
		V(as_bitmap_mutex);
	}

	/* Destroy the regions, pagetable, and as */
	kfree(as->as_code);
	kfree(as->as_data);
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
	int spl = splhigh();

	/* Allocate space for new addrspace */
	struct addrspace *new;
	new = as_create();
	if (new==NULL) {
		splx(spl);
		return ENOMEM;
	}

	/* Copy over virtual address fields */
	new->as_code->vbase = old->as_code->vbase;
	new->as_code->npages = old->as_code->npages;
	new->as_code->permissions = old->as_code->permissions;

	new->as_data->vbase = old->as_data->vbase;
	new->as_data->npages = old->as_data->npages;
	new->as_data->permissions = old->as_data->permissions;

	new->as_heapstart = old->as_heapstart;
	new->as_heapend = old->as_heapend;
	new->as_stackptr = old->as_stackptr;

	/* 
	 * Copy entries to the page table
	 * This is a deep copy, including all the pte entries. So the mappings for each page table
	 * are identical. This can be used to later implement copy on write
	 */
	err = pt_copy(old->as_pagetable, new->as_pagetable);
	if(err) {
		as_destroy(new);
		splx(spl);
		return ENOMEM;
	}

	/* Set every single virtual address mapping to 0, this will change when we do copy on write */
	vaddr_t vaddr = 0;
	while(1) {
		/* Should never be a page with vaddr_t 0 */
		vaddr = pt_getnext(new->as_pagetable, vaddr);
		if(vaddr==0) {
			break; /* Been through all the pages */
		}
		assert(vaddr < USERTOP); /* Should not ever go over user virtual address space */

		/* Allocate a physical page for this pte */
		struct pte *entry = pt_get(new->as_pagetable, vaddr);
		entry->ppageaddr = 0;
	}

	/* 
	 * Allocate a new physical page for every virtual page in the page table
	 * This will be deprecated later when we do copy on write!
	 */
	while(1) {
		/* Should never be a page with vaddr_t 0 */
		vaddr = pt_getnext(new->as_pagetable, vaddr);
		if(vaddr==0) {
			break; /* Been through all the pages */
		}
		assert(vaddr < USERTOP); /* Should not ever go over user virtual address space */

		/* Get PTE from old as and new as */
		struct pte *old_entry = pt_get(old->as_pagetable, vaddr);
		struct pte *new_entry = pt_get(new->as_pagetable, vaddr);
		assert(new_entry->ppageaddr == 0);

		/* Allocate a page */
		new_entry->ppageaddr = get_ppages(1, 0, 0); /* ask for 1 non-fixed user-page */
		if(new_entry->ppageaddr == 0) {
			as_destroy(new);
			splx(spl);
			return ENOMEM;
		}

		/* Update permissions */
		if( is_vaddrcode(new, vaddr) )
			new_entry->permissions = new->as_code->permissions;
		else if( is_vaddrdata(new, vaddr) )
			new_entry->permissions = new->as_data->permissions;
		else if( is_vaddrheap(new, vaddr) )
			new_entry->permissions = set_permissions(1, 1, 0); /* Heap permissions */
		else if( is_vaddrstack(new, vaddr) )
			new_entry->permissions = set_permissions(1, 1, 0); /* Stack permissions */
		else {
			panic("Unknown region. Memory is not managed properly.");
		}

		/*
		* Copy contents of all pages
		* We do this by converting all physical pages to kernel addresses, then using memmove()
		*/
		memmove((void *)PADDR_TO_KVADDR(old_entry->ppageaddr), 
				(const void *)PADDR_TO_KVADDR(new_entry->ppageaddr),
				PAGE_SIZE);
			
		assert(old_entry->ppageaddr != new_entry->ppageaddr);
	}

	// pt_dump(new->as_pagetable);

	*ret = new;
	splx(spl);
	return 0;
}


/*
 * as_prepare_load()
 * This function sets up the page table and allocates all the pages
 * This is called in load_elf()
 * 
 * This function will most likely be deprecated when we start doing load on demand
 * 
 * Note: Permissions are set to RWX. In complete load we set them to what they should be.
 * This is because we need to actually load the code and data segments into memory first,
 * which requires RWX priviledges.
 */ 
int
as_prepare_load(struct addrspace *as)
{	
	/* Do a sanity check */
	assert(as->as_code->npages != 0);
	assert(as->as_code->vbase != 0);

	int spl = splhigh();

	/* 
	 * We have to allocate memory for code and data segments
	 * Do this by adding entries into the page table
	 */
	unsigned i;
	struct pte *entry;
	vaddr_t vpageaddr;

	/* Code segment */
	for(i=0; i<as->as_code->npages; i++) {	
		vpageaddr = (as->as_code->vbase + (i*PAGE_SIZE));
		entry = alloc_upage(as, vpageaddr);
		if(entry == NULL) {
			return ENOMEM;
		}
		entry->permissions = set_permissions(1, 1, 1); /* RWX */
		//kprintf("Allocated space for code page #%u\n", i);
	}

	/* Data segment */
	for(i=0; i<as->as_data->npages; i++) {
		vpageaddr = (as->as_data->vbase + (i*PAGE_SIZE));
		entry = alloc_upage(as, vpageaddr);
		if(entry == NULL) {
			return ENOMEM;
		}
		entry->permissions = set_permissions(1, 1, 1); /* RWX */
		//kprintf("Allocated space for data page #%u\n", i);
	}

	splx(spl);
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

	if(!DEBUG_ASID_ENABLE) {
		TLB_Flush();
	}
	else {
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
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	if(as->as_code->vbase == 0){
		as->as_code->vbase = vaddr;
		as->as_code->npages = npages;
		as->as_code->permissions = set_permissions(readable, writeable, executable);
		return 0;		
	}

	if(as->as_data->vbase == 0){
		as->as_data->vbase = vaddr;
		as->as_data->npages = npages;
		as->as_data->permissions = set_permissions(readable, writeable, executable);
		return 0;		
	}

	panic("Too many regions! Not supported");
	return 0;
}


/*
 * Set the permissions to the proper values.
 * This is called after all the segments are loaded
 */
int
as_complete_load(struct addrspace *as)
{
	unsigned i;
	struct pte *entry;
	vaddr_t addr;

	/* Code segment */
	for(i=0; i<as->as_code->npages; i++) {
		addr = (as->as_code->vbase + (i*PAGE_SIZE)); 
		entry = pt_get(as->as_pagetable, addr);
		entry->permissions = as->as_code->permissions;
	}

	/* Data segment */
	for(i=0; i<as->as_data->npages; i++) {
		addr = (as->as_data->vbase + (i*PAGE_SIZE)); 
		entry = pt_get(as->as_pagetable, addr);
		entry->permissions = as->as_data->permissions;
	}

	/* Flush TLB as all TLB entries are dirty */
	//TLB_Flush();

	return 0;
}

/*
 * Initialize the stack pointer
 */
int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/* Initial user-level stack pointer */
	as->as_stackptr = USERTOP;
	*stackptr = USERSTACK;
	
	return 0;
}

/* 
 * Initialize the heap
 */
void
as_define_heap(struct addrspace *as) {
	as->as_heapstart = as->as_data->vbase + PAGE_SIZE*as->as_data->npages;
	as->as_heapend = as->as_heapstart;
}


/* Simple helpers to determine the region of memory that we reside in */
int is_vaddrcode(struct addrspace *as, vaddr_t vaddr)
{
	vaddr_t region_start = as->as_code->vbase;
	vaddr_t region_end = as->as_code->vbase + (as->as_code->npages)*PAGE_SIZE;
	if( (vaddr >= region_start) && (vaddr < region_end) ) {
		return 1;
	}
	return 0;
}

int is_vaddrdata(struct addrspace *as, vaddr_t vaddr)
{
	vaddr_t region_start = as->as_data->vbase;
	vaddr_t region_end = as->as_data->vbase + (as->as_data->npages)*PAGE_SIZE;
	if( (vaddr >= region_start) && (vaddr < region_end) ) {
		return 1;
	}
	return 0;
}

int is_vaddrheap(struct addrspace *as, vaddr_t vaddr)
{
	vaddr_t region_start = as->as_heapstart;
	vaddr_t region_end = as->as_heapend;
	if( (vaddr >= region_start) && (vaddr < region_end) ) {
		return 1;
	}
	return 0;
}

int is_vaddrstack(struct addrspace *as, vaddr_t vaddr)
{
	vaddr_t region_start = as->as_stackptr;
	vaddr_t region_end = USERSTACK;
	if( (vaddr >= region_start) && (vaddr < region_end) ) {
		return 1;
	}
	return 0;
}


/* Debug function */
void region_dump(struct addrspace *as) 
{
#if !OPT_DUMBVM

	unsigned i, j;
	u_int32_t *vaddr;
	struct pte *entry;

	int spl = splhigh();
	/* Print segments */
	kprintf("Printing Code Segment \n\n");
	for(i=0; i<as->as_code->npages; i++) {
		kprintf("Page %d:\n", i);
		/* Get the physical page */
		entry = pt_get(as->as_pagetable, (as->as_code->vbase+i*PAGE_SIZE) );
		assert(entry != NULL);

		/* Convert address to kernel virtual address and cast it to a pointer */
		vaddr = (u_int32_t *)PADDR_TO_KVADDR(entry->ppageaddr);

		/* 4096/32 = 128 */
		for(j=0; j<(PAGE_SIZE/sizeof(u_int32_t)); j++) {
			kprintf("%x", vaddr[j]);
		}
		kprintf("\n");
	}
	splx(spl);

#else

	unsigned i, j;
	u_int32_t *vaddr;

	int spl = splhigh();

	/* Print segments */
	kprintf("Printing Code Segment \n\n");
	for(i=0; i<as->as_npages1; i++) {
		kprintf("Page %d:\n", i);
		
		/* Convert address to kernel virtual address and cast it to a pointer */
		vaddr = (u_int32_t *)PADDR_TO_KVADDR(as->pbase1 + i*PAGE_SIZE);

		/* 4096/32 = 128 */
		for(j=0; j<(PAGE_SIZE/sizeof(u_int32_t)); j++) {
			kprintf("%x", vaddr[j]);
		}
		kprintf("\n");
	}
	
	splx(spl);

#endif

}


