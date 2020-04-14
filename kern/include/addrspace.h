
/* 
 * Address space - data structure associated with the virtual memory
 * space of a process.
 */

#ifndef _ADDRSPACE_H_
#define _ADDRSPACE_H_

#include <vm.h>
#include <pagetable.h>
#include <permissions.h>
#include <uio.h>
#include <elf.h>
#include <curthread.h>


struct vnode;

/*
 * Definition of as_region
 * 
 * 4 regions in the user address space: code, data, heap, and stack
 * Each region has its own start address and size as well as a set of permissions
 * Permission types: executable, writable, and readable.
 * We keep new and old flags for now for some reason. We can get rid of them later if needed.
 */
struct as_region {
	vaddr_t vbase;
	size_t npages;
	permissions_t permissions;
	/* Will need a lot more information for ondemand paging */
	struct vnode *file;
	struct uio uio;
};
/* define the region */
void region_dump(struct addrspace *as) ;

/* Address space ID typdef */
typedef u_int32_t asid_t;

/*
 * Definition of addrspace:
 * 
 * User processes are given virtual addresses ranging from: MIPS_KUSEG -> MIPS_KSEG0 (0x00000000 -> 0x80000000)
 * In these address spaces, we need leave space for the following:
 * 		(1) Code segment
 * 		(2) Data segment
 * 		(3) Heap
 * 		(4) Stack
 * 
 * There are a few ways in that our VM system is different from DUMBVM
 * 		(1) To support user level malloc(), we need allocate space for the heap
 * 		(2) Vpages are mapped to Ppages using page tables, so each addrspace will need its own page table
 * 		(3) Because of continuous swapping, we won't keep track of physical addresses as they can change 
 * 			upon eviction of a page. Page tables can be referenced to do the address translation when needed.
 */
struct addrspace {
#if OPT_DUMBVM
	/* DUMBVM */
	vaddr_t as_vbase1;		/* base vaddr of code segment */
	paddr_t as_pbase1;		/* base paddr of code segment */
	size_t as_npages1;		/* num_pages used by code segment */
	vaddr_t as_vbase2;		/* base vaddr of data segment */
	paddr_t as_pbase2;		/* base paddr of data segment */
	size_t as_npages2;		/* num_pages used by data segment */
	paddr_t as_stackpbase;	/* base paddr of stack */
#else
	/* If gypsies moved into the VM business */
	pagetable_t as_pagetable;	/* page table */
	struct as_region *as_code;	/* code segment */
	struct as_region *as_data;	/* data segment */
	vaddr_t as_heapstart;		/* start of heap */
	vaddr_t as_heapend;			/* end of heap */
	vaddr_t as_stackptr;		/* stackptr */
	asid_t as_asid;				/* addrspace tags for the TLB */
	int as_asid_set;
#endif
};


/*
 * Functions in addrspace.c:
 *    
 *    as_bitmap_bootstrap - initializes the bitmap for ASIDs and 
 * 							  the mutex for the structure
 *    as_create - create a new empty address space. You need to make 
 *                sure this gets called in all the right places. You
 *                may find you want to change the argument list. May
 *                return NULL on out-of-memory error.
 *
 *    as_copy   - create a new address space that is an exact copy of
 *                an old one. Probably calls as_create to get a new
 *                empty address space and fill it in, but that's up to
 *                you.
 *
 *    as_activate - make the specified address space the one currently
 *                "seen" by the processor. Argument might be NULL, 
 *		  meaning "no particular address space".
 *
 *    as_destroy - dispose of an address space. You may need to change
 *                the way this works if implementing user-level threads.
 *
 *    as_define_region - set up a region of memory within the address
 *                space.
 *
 *    as_prepare_load - this is called before actually loading from an
 *                executable into the address space.
 *
 *    as_complete_load - this is called when loading from an executable
 *                is complete.
 *
 *    as_define_stack - set up the stack region in the address space.
 *                (Normally called *after* as_complete_load().) Hands
 *                back the initial stack pointer for the new process.
 */
void              as_bitmap_bootstrap(void);
struct addrspace *as_create(void);
int               as_copy(struct addrspace *src, struct addrspace **ret);
void              as_activate(struct addrspace *);
void              as_destroy(struct addrspace *);

int               as_define_region(struct addrspace *as, 
				   	vaddr_t vaddr, size_t sz,
				   	int readable, 
				   	int writeable,
				   	int executable);

int		  		  as_prepare_load(struct addrspace *as);
int		  		  as_complete_load(struct addrspace *as);
int               as_define_stack(struct addrspace *as, vaddr_t *initstackptr);
void 			  as_define_heap(struct addrspace *as);

int is_vaddrcode(struct addrspace *as, vaddr_t vaddr);
int is_vaddrdata(struct addrspace *as, vaddr_t vaddr);
int is_vaddrheap(struct addrspace *as, vaddr_t vaddr);
int is_vaddrstack(struct addrspace *as, vaddr_t vaddr);


/*
 * Functions in loadelf.c
 *    load_elf - load an ELF user program executable into the current
 *               address space. Returns the entry point (initial PC)
 *               in the space pointed to by ENTRYPOINT.
 *    load_elf_od - load an ELF user program executable on demand.
 * 					This means we don't allocate all the pages at once, only
 * 					on page faults
 */

int load_elf(struct vnode *v, vaddr_t *entrypoint);

int load_elf_od(struct vnode *v, vaddr_t *entrypoint);
int load_segment_od(struct vnode *v, off_t offset, vaddr_t vaddr, size_t memsize, size_t filesize, int is_executable);
int load_page_od(struct vnode *v, struct uio u, off_t p_offset);

#endif /* _ADDRSPACE_H_ */
