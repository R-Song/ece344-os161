
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
#include <vm.h>
#include <kern/errno.h>


/* the actual name of the file. Read the man page for more information */
static const char *swap_fname = "lhd1raw:";

/* statistics about the swap file */
static struct stat *swap_fstat;

/* abstract representation of the swap file */
static struct vnode *swap_vnode;

/* synchronization device to make sure only one thread is swapping at any one time */
struct lock *swap_lock;

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
    bitmap_mark(swap_bitmap, 0); /* mark the first index, this one should not be used */
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
 * swap_pageevict()
 * 
 * Eviction is the process of officially removing a page from memory.
 * After a page it is evicted, its ppageaddr is set to 0, and its swap_state is set to PTE_SWAPPED.
 * Only clean pages can be evicted! Once evicted, the TLB entry is also shot down, as the translation
 * is not invalid.
 */
void swap_pageevict(struct pte *entry)
{
    assert(curspl>0);
    assert(lock_do_i_hold(swap_lock));

    assert(entry->swap_state == PTE_CLEAN);
    assert(entry->ppageaddr != 0);

    /* Free the physical page and change the state of the entry */
    free_ppages(entry->ppageaddr);
    entry->ppageaddr = 0;
    entry->swap_state = PTE_SWAPPED;
 
    TLB_Flush(); /* Shoot down the specific TLB entry later, for now we just flush */
}

/* 
 * swap_pageout()
 * 
 * Find a appropriate page to swap out. Find a spot in the swap disk and write the contents
 * of the physical page to the swap disk. Once this is done, the page is clean. We can then
 * evict this page.
 * 
 * Currently the eviction policy used is nMRU
 * 
 * Returns 0 on success.
 */
int swap_pageout()
{
    int err;
    u_int32_t swap_location;
    struct pte *entry_to_swap;

    int spl = splhigh();
    assert( lock_do_i_hold(swap_lock) );
    
    /* We have to find a target to swap out */
    entry_to_swap = coremap_swap_pageout();
    if(entry_to_swap == NULL) {
        splx(spl);
        return 1;
    }

    assert(entry_to_swap->swap_state != PTE_SWAPPED);
    assert(entry_to_swap->ppageaddr != 0);

    switch(entry_to_swap->swap_state) {
        case PTE_PRESENT:
            /* we have to allocate a place in swap memory and evict */
            err = bitmap_alloc(swap_bitmap, &swap_location);
            if(err) {
                splx(spl);
                return err;
            }

            /* write to this location */
            err = swap_write(swap_location, entry_to_swap->ppageaddr);
            if(err) {
                bitmap_unmark(swap_bitmap, swap_location);
                splx(spl);
                return err;
            }

            entry_to_swap->swap_location = swap_location;
            entry_to_swap->swap_state = PTE_CLEAN;
            swap_pageevict(entry_to_swap);
            break;

        case PTE_DIRTY:
            /* Dirty means that it already has a page in swap disk */
            swap_location = entry_to_swap->swap_location;

            err = swap_write(swap_location, entry_to_swap->ppageaddr);
            if(err) {
                splx(spl);
                return err;
            }

            entry_to_swap->swap_location = swap_location;
            entry_to_swap->swap_state = PTE_CLEAN;
            swap_pageevict(entry_to_swap);
            break;
        
        case PTE_CLEAN:
            swap_pageevict(entry_to_swap);
            break;

        default:
            panic("Invalid PTE state");
    }

    splx(spl);
    return 0;
}


/*
 * swap_pagein()
 * 
 * Bring a specific page back into memory. Do this by first allocating a page. Once we have
 * a page, we can start writing from the swap disk to the page in memory.
 * 
 */
int swap_pagein(struct pte *entry)
{
    int err = 0;
    int spl = splhigh();
    assert(lock_do_i_hold(swap_lock));

    assert(entry->swap_state == PTE_SWAPPED);
    assert(entry->ppageaddr == 0);

    /* Get a physical page for this entry */
    alloc_upage(entry);
    if(entry->ppageaddr == 0) {
        splx(spl);
        return ENOMEM;
    }
    assert(entry->ppageaddr != 0);

    err = swap_read(entry->swap_location, entry->ppageaddr);
    if(err) {
        splx(spl);
        return err;
    }

    entry->swap_state = PTE_CLEAN;      /* Just loaded the page, it is clean */

    splx(spl);
    return 0;
}

/*
 * Use swapping to free up npages of memory
 * Return 1 if space was created successfully, 0 if not
 */
int swap_createspace(int npages)
{
    int spl = splhigh();
    assert(lock_do_i_hold(swap_lock));

    int result;

    /* export the job to coremap */
    result = coremap_swap_createspace(npages);
    if(result) {
        splx(spl);
        return result;
    }

    splx(spl);
    return 0;
}

/*
 * This for when we want to allocate on stack or heap on demand
 * Basically just reserve a spot in swap disk and bring it in
 * whenever the user needs it!
 */
int swap_allocpage_od(struct pte *entry) 
{
    assert(lock_do_i_hold(swap_lock));
    assert(curspl>0);

    int err;

    u_int32_t swap_location;

    err = swap_allocpage(&swap_location);
    if(err) {
        return err;
    }

    entry->ppageaddr = 0;
    entry->swap_state = PTE_SWAPPED;
    entry->swap_location = swap_location;

    return 0;
}



/*
 * Work with the bitmap
 */
void swap_freepage(u_int32_t swap_location)
{
    assert( lock_do_i_hold(swap_lock) );
    bitmap_unmark(swap_bitmap, swap_location);
}

int swap_allocpage(u_int32_t *swap_location)
{
    assert( lock_do_i_hold(swap_lock) );
    int err;
    err = bitmap_alloc(swap_bitmap, swap_location);
    if(err) {
        return err;      /* swap is full! */
    }
    return 0;
}




