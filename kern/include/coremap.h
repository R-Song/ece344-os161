
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

struct pte;

/* 
 * ppagestate_t, physical page state type
 *
 * S_FREE: Page is not allocated    
 * S_USER: User allocated page
 * S_KERN: Direct mapped kernel pages
 */
typedef enum {
    S_FREE,
    S_USER,
    S_KERN,
} ppagestate_t;


/* 
 * Definition of coremap_entry
 * Only for user pages do we care about the owner thread. 
 * Kernel pages are always in state S_FIXED and are directly mapped from virtual address to physical.
 */
struct coremap_entry {
    /* state of the physical page (ppage) */
    ppagestate_t state;

    /* number of pages allocated. This number is only useful for the first page in a chain of allocated pages */
    int num_pages_allocated;

    /* age table entry associated with this coremap entry. NULL if this is a kernel entry */
    struct pte *pt_entry;

    /* referenced bit used for lru clock page evict algo */
    int referenced;
};


/*
 * Functions that provide abstraction to the coremap structure
 * Read coremap.c for more information on how each function is implemented
 */

extern struct coremap_entry *coremap;

/* Initialize coremap structure */
void    coremap_bootstrap();

/* Allocates a physical page if available */
paddr_t get_ppages(int npages, int is_kernel, struct pte *pt_entry);

/* Deallocate a physical page */
void    free_ppages(paddr_t paddr);

/* Debugging */
void    coremap_stat();

/* Functions to help with swapping */
struct pte *coremap_swap_pageout();
int         coremap_swap_createspace(int npages);

void coremap_lruclock_update(paddr_t ppageaddr);

#endif /* _COREMAP_H_ */
