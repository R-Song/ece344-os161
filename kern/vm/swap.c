
/*
 * Implementation of swapping.
 * All code is protected by disabling interrupts as well as acquiring the swap lock.
 */

#include <types.h>
#include <swap.h>
#include <vfs.h>
#include <uio.h>
#include <vnode.h>
#include <kern/stat.h>
#include <kern/unistd.h>
#include <machine/vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <synch.h>
#include <bitmap.h>
#include <coremap.h>
#include <pagetable.h>
#include <lib.h>


/* the actual name of the file. Read the man page for more information */
static const char *swap_fname = "lhd1raw:";

/* statistics about the swap file */
static struct stat *swap_fstat;

/* abstract representation of the swap file */
static struct vnode *swap_vnode;

/* synchronization device to make sure only one thread is swapping at any one time */
static struct lock *swap_lock;

/* structure to keep track of each swap location */
static struct bitmap *swap_bitmap;

/* counter for how many pages are available for swapping */
static u_int32_t num_swap_pages_avail;


/*
 * swap_bootstrap()
 * Initializes all data structures to keep track of swapfile:
 *      1. swap lock
 *      2. bit map to keep track of storage
 *      3. varable to keep track of how many pages we have left
 * To determine how big our bitmap should be, we should open up the swap file and check
 */
void swap_bootstrap() 
{
    /* create the swap lock */
    swap_lock = lock_create("swap_lock");
    if(swap_lock == NULL) {
        panic("Could not create swap lock");
    }

    /* open the swap file */
    int err = vfs_open((char *)swap_fname, O_RDWR, &swap_vnode); /* open swap file with read and write priviledges */
    if(err) {
        panic("Could not open swap file");
    }

    /* figure out how many pages we have available to swap */
    swap_fstat = (struct stat *)kmalloc(sizeof(struct stat));
    if(swap_fstat == NULL) {
        panic("Could not create swap stats");
    }
    VOP_STAT(swap_vnode, swap_fstat);
    num_swap_pages_avail = (swap_fstat->st_size >> PAGE_OFFSET);

    /* Create the bitmap for book keeping */
    swap_bitmap = bitmap_create(num_swap_pages_avail);
    if(swap_bitmap == NULL) {
        panic("Could not create swap bitmap");
    }
}


/* Given a swapfile location, read the swap page into physical page */
int swap_read(u_int32_t swap_location, paddr_t ppage)
{
    assert( lock_do_i_hold(swap_lock) );
    assert(curspl>0)

    int err;

    /* initialize uio */
    struct uio ku;
    off_t offset = swap_location * PAGE_SIZE;
    mk_kuio(&ku, (void *)PADDR_TO_KVADDR(ppage), PAGE_SIZE, offset, UIO_READ);

    /* read from swap disk into memory */
    err = VOP_READ(swap_vnode, &ku);
    if(err) {
        return err;
    }

    return 0;
}

/* Given a swapfile location, write the physical page into the swap location */
int swap_write(u_int32_t swap_location, paddr_t ppage)
{
    assert( lock_do_i_hold(swap_lock) );
    assert(curspl>0)

    int err;

    /* initialize uio */
    struct uio ku;
    off_t offset = swap_location * PAGE_SIZE;
    mk_kuio(&ku, (void *)PADDR_TO_KVADDR(ppage), PAGE_SIZE, offset, UIO_WRITE);

    /* read from swap disk into memory */
    err = VOP_WRITE(swap_vnode, &ku);
    if(err) {
        return err;
    }

    return 0;
}

/* 
 * For now, the implementation is more simple
 * There is only one copy on disk or memory at any one time. This means that
 * the clean state in the pte and coremap are never used. Will change later
 */


/* 
 * Copy the page from memory to disk. Update status of page on coremap 
 */
int swap_pageout(struct pte *entry)
{
    int err = 0;

    int spl = splhigh();
    lock_acquire(swap_lock);

    /* list of assertions for sanity check */
    assert(entry != NULL);
    assert(entry->is_swapped == 0);
    assert(entry->ppageaddr != 0);

    /* mark the bitmap */
    u_int32_t swap_location;
    err = bitmap_alloc(swap_bitmap, &swap_location);
    if(err) {
        goto swap_pageout_done;      /* swap is full! */
    }

    /* write to the swap file */
    err = swap_write(swap_location, entry->ppageaddr);
    if(err) {
        bitmap_unmark(swap_bitmap, swap_location);
        goto swap_pageout_done; 
    }

    /* update the entry */
    entry->swap_location = swap_location;
    entry->is_swapped = 1;

swap_pageout_done:
    lock_release(swap_lock);
    splx(spl);
    return err;
}


/*
 * Given a page table entry, read the contents of the page on 
 * the swap disk into entry->ppageaddr.
 * Prior to calling this, there must already be a ppage allocated for this entry
 */
int swap_pagein(struct pte *entry)
{
    int err = 0;
    int spl = splhigh();
    lock_acquire(swap_lock);

    /* assertions for sanity check */
    assert(entry != NULL);
    assert(entry->is_swapped == 1);
    assert(entry->ppageaddr != 0);
    
    err = swap_read(entry->swap_location, entry->ppageaddr);
    if(err) {
        goto swap_pagein_done;
    }

    bitmap_unmark(swap_bitmap, entry->swap_location);
    entry->is_swapped = 0;
    entry->is_present = 1;
    entry->swap_location = 0;

swap_pagein_done:
    lock_release(swap_lock);
    splx(spl);
    return err;
}

/*
 * Given a page table entry, evict the page from the coremap
 */
void swap_pageevict(struct pte *entry)
{
    int spl = splhigh();

    free_ppages(entry->ppageaddr);
    entry->is_present = 0;
    TLB_Flush(); /* Shoot down the specific TLB entry later, for now we just flush */

    splx(spl);
}

/*
 * Use swapping to free up npages of memory
 * Return 1 if space was created successfully, 0 if not
 */
int swap_createspace(int npages)
{
    int spl = splhigh();
    lock_acquire(swap_lock);

    int result;

    /* export the job to coremap */
    result = coremap_swaphelper(npages);
    if(result) {
        lock_release(swap_lock);
        splx(spl);
        return 1;
    }

    TLB_Flush(); /* For now we just flush */

    lock_release(swap_lock);
    splx(spl);
    return 0;
}

/*
 * Deallocate a page that is swapped
 */
void swap_freepage(u_int32_t swap_location)
{
    bitmap_unmark(swap_bitmap, swap_location);
}


