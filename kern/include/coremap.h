
/* 
 * Definition of the coremap, fundamental structure in the VM system.
 * 
 * Coremap is a fundamental component of the VM system. It is essentially a reverse page table. The number of entries
 * in the coremap is exactly the number of physical pages available for use. Each entry has information such as the owner
 * thread, virtual page mapping, and state of the page. The coremap is basically an array indexed by the physcical page number
 * 
 */

#ifndef _COREMAP_H_
#define _COREMAP_H_

/* 
 * Defining the states a page can be in.
 * 
 * S_FREE:       
 * S_DIRTY:
 * S_FIXED:
 * S_CLEAN:
 */
typedef enum {
    S_FREE,
    S_DIRTY,
    S_FIXED,
    S_CLEAN,
} pagestate_t;


/* Definition of coremap_entry */
struct coremap_entry {
    /* 
     * Have to keep track of who owns this page 
     * Through the thread pointer, we can access all the information we might possibly need
     */
    struct thread *owner;
    
    /* Virtual page number */
    int vpage_num;

    /* Add more information as we go */
    pagestate_t state;
};


/* 
 * coremap_bootstrap()
 * 
 * Initializes coremap structure, called in vm_bootstrap(). 
 * This is a little different than other bootstrap functionsbecause it doesn't use kmalloc(). 
 * Instead, the coremap table is allocated using the machine dependant steal_ram function.
 */
void coremap_bootstrap();

/* 
 * get_ppage()
 * 
 * Gets physical page if available. 
 */



#endif /* _COREMAP_H_ */