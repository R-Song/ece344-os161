
#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <machine/spl.h>
#include <coremap.h>

/* coremap structure, an array allocated at runtime */
static struct coremap_entry *coremap = NULL;

/* synchronization primitive for the coremap */
static struct semaphore *coremap_mutex = NULL;

/* first available page. All pages up to and including the coremap are fixed, and should not be deallocated... */
static int first_avail_ppage = 0;

/* one after the last available page */
static int last_avail_ppage = 0;

/* flag to indicate whether or not to use mutex, for debugging */
static int DEBUG_USE_MUTEX = 0; 
/* 
 * Currently mutex doesn't work, but I want to eventually use a synchronization primitive rather than interrupts... 
 * Using the semaphore causes a hang in booting... not sure where the deadlock is coming from
 */


/*
 * coremap_bootstrap()
 * 
 * Initializes the coremap structure. This should be the first thing to be called after ram_bootstrap().
 * This function does the following neccessary things:
 *      (1) use ram_getsize() to understand how much memory we have, in the process firstppaddr and lastppaddr are both destroyed
 *      (2) Calculate how many pages we can fit, given that we need space for the coremap
 *      (3) Allocate space for the coremap manually using physical address pointers
 *      (4) Create the coremap_mutex, kmalloc should work by this point
 */
void coremap_bootstrap() 
{
    int i;
    int spl = splhigh();    /* Turn off interrupts */

    /* Use ram_getsize() to retrieve how much memory we have */
    paddr_t firstpaddr, lastpaddr;
    ram_getsize(&firstpaddr, &lastpaddr);

    /* Total number of physical pages */
    int num_ppages = (lastpaddr >> PAGE_OFFSET);

    /* Number of pages dedicated to fitting the coremap and its mutex */
    int num_coremap_pages = ( (num_ppages*sizeof(struct coremap_entry) + sizeof(struct semaphore) + PAGE_SIZE) >> PAGE_OFFSET) ;

    /* Initialize address of coremap */
    coremap = (struct coremap_entry *)PADDR_TO_KVADDR(firstpaddr);

    /* All pages before firstpaddr are super important and should not be displaced! */
    int num_fixed_pages = ((firstpaddr >> PAGE_OFFSET) + num_coremap_pages);

    for(i=0; i<num_fixed_pages; i++) {
        coremap[i].owner = NULL;
        coremap[i].vpage_num = (PADDR_TO_KVADDR(i*PAGE_SIZE) >> PAGE_OFFSET); /* Kernel VP mapping */
        coremap[i].state = S_FIXED; /* This memory is fixed */
        coremap[i].num_pages_allocated = 1;
    }

    /* Initialize the rest of the coremap */
    for(i=num_fixed_pages; i<num_ppages; i++) {
        coremap[i].owner = NULL;
        coremap[i].vpage_num = 0;    /* Kernel VP mapping */
        coremap[i].state = S_FREE; /* This memory is fixed */
        coremap[i].num_pages_allocated = 1;
    }

    /* Initialize coremap_mutex */
    coremap_mutex = (struct semaphore *)PADDR_TO_KVADDR(firstpaddr + num_ppages*sizeof(struct coremap_entry));
	coremap_mutex->count = 1;

    /* save first and last pages */
    first_avail_ppage = num_fixed_pages;
    last_avail_ppage = num_ppages;

    splx(spl); 
}


/*
 * get_ppage()
 * 
 * Find the first free consecutive npages and allocate it for the current thread.
 * We are looking for consecutive physical pages. If found, we update the coremap accordingly and 
 * return the physical address of the first page allocated.
 * 
 * is_fixed=1 means we want this page allocation to be fixed in memory, never swapped.
 * is_kernel=1 means we are allocating a kernel page, meaning its virtual page number is directly mapped.
 */
paddr_t get_ppages(int npages, int is_fixed, int is_kernel) 
{   
    /* local variables */
    int page_it = 0;
    int cnt = 0;
    int space_avail = 0;
    int spl;
    
    /* get access to coremap using semapore or just disable interrupts */
    if(!DEBUG_USE_MUTEX) 
        spl = splhigh();
    else {
        P(coremap_mutex);
    }

    /* loop through available pages and find consevutively free pages */
    for(page_it=first_avail_ppage; page_it<last_avail_ppage; page_it++){
        /* check if space is available */
        if(cnt == npages) {
            space_avail = 1;
            break;
        } 
        /* look through coremap for consecutive free pages */   
        if(coremap[page_it].state == S_FREE)
            cnt++;
        else {
            cnt = 0;
        }
    }

    if(space_avail) {
        /* Space was found, so we allocate it on the coremap */
        int i;
        int start_page = page_it - npages;
        int end_page = start_page + npages;

        for(i=start_page; i<end_page; i++) {
            coremap[i].owner = curthread;

            if(is_fixed) 
                coremap[i].state = S_FIXED;
            else {
                coremap[i].state = S_DIRTY;
            }

            coremap[i].is_kernel = is_kernel;

            if(is_kernel)
                coremap[i].vpage_num = (PADDR_TO_KVADDR(i*PAGE_SIZE) >> PAGE_OFFSET);
            else {
                coremap[i].vpage_num = 0; /* When do we do the virtual address mapping??? Maybe later? */
            }

            if(i==start_page)
                coremap[i].num_pages_allocated = npages;
            else {
                coremap[i].num_pages_allocated = 0;
            }
        }

        if(!DEBUG_USE_MUTEX) 
            splx(spl);
        else {
            V(coremap_mutex);
        }
        return (start_page*PAGE_SIZE);
    }

    if(!DEBUG_USE_MUTEX) 
        splx(spl);
    else {
        V(coremap_mutex);
    }
    return 0;
}


/*
 * free_ppages()
 * 
 * Frees consecutively allocated pages. We know that they are consecutive because we saved
 * them when allocated them :) big brain.
 */
void free_ppages(paddr_t paddr) 
{   
    int spl;

    /* get access to coremap using semapore or just disable interrupts */
    if(!DEBUG_USE_MUTEX) 
        spl = splhigh();
    else {
        P(coremap_mutex);
    }

    /* Get the start and end pages to free */
    int start_page = (paddr >> PAGE_OFFSET);
    int end_page = start_page + coremap[start_page].num_pages_allocated;
    if(start_page == end_page) {
        panic("Fatal Coremap: Probably double freeing...");
    }
    assert(start_page>=first_avail_ppage && end_page<last_avail_ppage);

    /* Go through the coremap entries and free neccessary entries */
    int i;
    for(i=start_page; i<end_page; i++) {
        assert(coremap[i].state != S_FREE);
        coremap[i].owner = NULL;
        coremap[i].state = S_FREE;
        coremap[i].is_kernel = 0;
        coremap[i].vpage_num = 0;
        coremap[i].num_pages_allocated = 0;
    }
    
    if(!DEBUG_USE_MUTEX) 
        splx(spl);
    else {
        V(coremap_mutex);
    }
}


/*
 * coremap_stat()
 * Print relevant information about the coremap for debugging
 */
void coremap_stat() {
    int spl = splhigh();
    int i;
    int j = 0;
    /* Print status of all pages */
    kprintf("COREMAP STATUS DUMP:\n");
    for(i=first_avail_ppage; i<last_avail_ppage; i++) {
        kprintf("P%d: ", i);
        if(coremap[i].state == S_FREE)
            kprintf("FREE    ");
        else if(coremap[i].state == S_DIRTY)
            kprintf("DIRTY   ");
        else if(coremap[i].state == S_FIXED)
            kprintf("FIXED   ");
        else if(coremap[i].state == S_CLEAN)
            kprintf("CLEAN   ");

        j++;

        if(j>7) {
            kprintf("\n");
            j = 0;
        }
    }
    kprintf("\n\n");

    splx(spl);
}

