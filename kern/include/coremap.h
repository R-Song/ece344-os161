
/* 
 * Definition of the coremap, fundamental structure in the VM system.
 * 
 * Coremap is a fundamental component of the VM system. It is essentially an array that maps physical addresses to virtual addresses.
 * The index to this array is the physical page number. Each entry to this array contains information such as the owner thread, 
 * physical page state(free, dirty, fixed, clean), virtual page mapping, (add more later).
 * 
 * Some notes about virtual addresses:
 *      * all virtual addresses above KSEG0(0x80000000) belong solely to the kernel and are direct mapped.
 *        This means that VA = PA + KSEG0 or, VPagenum = PPagenum + 0x80000.
 *        This memory is shared amongst the entire kernel and therefore has no notion of an owner thread.
 *        As a result, if VA greater than 0x80000000 we require no page table, and as a result, no TLB entry!
 *      * all addresses less than KSEG0 are considered to be in the user space. These addresses depend on the owner thread
 *        and require page tables, as they are no directly mapped. These entries require TLB entries!
 */

#ifndef _COREMAP_H_
#define _COREMAP_H_

/* 
 * ppagestate_t, physical page state type
 *
 * S_FREE: Page is not allocated    
 * S_DIRTY: Contents of page are not consistent with swap disk
 * S_CLEAN: Page is allocated and which has contents that agree with swap disk
 * S_FIXED: pages that should not ever be swapped out. These are kernel pages (direct mapped) and user code/data segments
 */
typedef enum {
    S_FREE,
    S_DIRTY,
    S_FIXED,
    S_CLEAN,
} ppagestate_t;


/* 
 * Definition of coremap_entry
 * Only for user pages do we care about the owner thread. 
 * Kernel pages are always in state S_FIXED and are directly mapped from virtual address to physical.
 */
struct coremap_entry {
    /* for user pages, we care about the owner thread to ensure protection from other processes */
    struct thread *owner;
    
    /* virtual page number */
    int vpage_num;

    /* state of the physical page (ppage) */
    ppagestate_t state;

    /* number of pages allocated. This number is only useful for the first page in a chain of allocated pages */
    int num_pages_allocated;

    /* Add more information as we go */
};


/*
 * Functions that provide abstraction to the coremap structure
 * Read coremap.c for more information on how each function is implemented
 */

/* Initialize coremap structure */
void coremap_bootstrap();
void coremap_mutex_bootstrap();

/* Allocates a physical page if available */
paddr_t get_ppages(int npages, int is_fixed, int is_kernel);

/* Deallocate a physical page */
void free_ppages(paddr_t paddr);

/* Add more functions later to implement other state transitions, flush, write... */


#endif /* _COREMAP_H_ */

