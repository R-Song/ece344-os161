
#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <machine/spl.h>
#include <coremap.h>
#include <pagetable.h>
#include <swap.h>

/* coremap structure, an array allocated at runtime */
static struct coremap_entry *coremap = NULL;

/* first available page. All pages up to and including the coremap are fixed, and should not be deallocated... */
static int first_avail_ppage = 0;

/* one after the last available page */
static int last_avail_ppage = 0;


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
    int num_coremap_pages = ( (num_ppages*sizeof(struct coremap_entry) + PAGE_SIZE-1) >> PAGE_OFFSET) ;

    /* Initialize address of coremap */
    coremap = (struct coremap_entry *)PADDR_TO_KVADDR(firstpaddr);

    /* All pages before firstpaddr are super important and should not be displaced! */
    int num_fixed_pages = ((firstpaddr >> PAGE_OFFSET) + num_coremap_pages);

    for(i=0; i<num_fixed_pages; i++) {
        coremap[i].state = S_FIXED; /* This memory is fixed */
        coremap[i].num_pages_allocated = 1;
        coremap[i].pt_entry = NULL;
    }

    /* Initialize the rest of the coremap */
    for(i=num_fixed_pages; i<num_ppages; i++) {
        coremap[i].state = S_FREE; /* This memory is free */
        coremap[i].num_pages_allocated = 1;
        coremap[i].pt_entry = NULL;
    }

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
 * is_kernel=1 means we are allocating a kernel page, meaning its virtual page number is directly mapped.
 */
paddr_t get_ppages(int npages, int is_kernel, struct pte *entry) 
{   
    /* local variables */
    int page_it = 0;
    int cnt = 0;
    int space_avail = 0;
    int spl;
    
    /* get access to coremap using semapore or just disable interrupts */
    spl = splhigh();

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
            if(is_kernel) {
                coremap[i].state = S_FIXED;
                coremap[i].pt_entry = NULL;
            }
            else {
                coremap[i].state = S_DIRTY;
                coremap[i].pt_entry = entry;
            }

            if(i==start_page)
                coremap[i].num_pages_allocated = npages;
            else {
                coremap[i].num_pages_allocated = 0;
            }
        }

        splx(spl);

        return (start_page*PAGE_SIZE);
    }

    splx(spl);

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
    spl = splhigh();


    /* Get the start and end pages to free */
    int start_page = (paddr >> PAGE_OFFSET);
    int end_page = start_page + coremap[start_page].num_pages_allocated;
    if(start_page == end_page) {
        panic("Fatal Coremap: Probably double freeing...\n");
    }
    assert(start_page>=first_avail_ppage && end_page<last_avail_ppage);

    /* Go through the coremap entries and free neccessary entries */
    int i;
    for(i=start_page; i<end_page; i++) {
        assert(coremap[i].state != S_FREE);
        coremap[i].state = S_FREE;
        coremap[i].num_pages_allocated = 0;
        coremap[i].pt_entry = NULL;
    }
    
    splx(spl);
}


/*
 * coremap_stat()
 * Print relevant information about the coremap for debugging
 */
void coremap_stat() 
{
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


/*
 * coremap_swaphelper()
 * helps swap find contiguous non-fixed, then swaps them out into memory.
 * if contiguous memory cannot to be found, it means we have too many kernel pages....
 */
int coremap_swaphelper(int npages) 
{
    assert(curspl>0);
    int err;
    int page_it = 0;
    int cnt = 0;
    int space_avail = 0;

    /* loop through available pages and find consecutively non-fixed pages */
    for(page_it=first_avail_ppage; page_it<last_avail_ppage; page_it++){
        /* check if space is available */
        if(cnt == npages) {
            space_avail = 1;
            break;
        } 
        /* look through coremap for consecutive free pages */   
        if(coremap[page_it].state != S_FIXED)
            cnt++;
        else {
            cnt = 0;
        }
    }

    if(space_avail) {
        /* Space was found, swap it out of the coremap to free it up*/
        int i;
        int start_page = page_it - npages;
        int end_page = start_page + npages;

        for(i=start_page; i<end_page; i++) {
            if(coremap[i].state == S_FREE) {
                continue;
            }

            assert(coremap[i].num_pages_allocated == 1); /* All pages that make it here should be user pages. */

            /* swap out the page */
            struct pte *entry;
            entry = coremap[i].pt_entry;
            //kprintf("COREMAP:\n%d\n", entry->is_swapped);
            err = swap_pageout(entry);
            if(err) {
                kprintf("failed to swap page out in coremap_swaphelper");
                return 0;
            }
            entry->is_present = 0;
            entry->ppageaddr = 0;

            /* free this coremap entry */
            coremap[i].state = S_FREE;
            coremap[i].pt_entry = NULL;
            coremap[i].num_pages_allocated = 0;
        }
        return 1;
    }

    return 0;
}







