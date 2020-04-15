#ifndef _SWAP_H_
#define _SWAP_H_

/*
 * Implementation of the page swap system. Includes a few parts:
 *     1) Data structure to locate specific pages on the swap file
 *     2) Interface with the swap file to allow read and writes to swapfile pages
 *     3) Interface to swap pages in and out of the coremap, keeping pagetables and coremap updated
 *     4) Method to make pages available for the kernel on large allocations
 * 
 * layers of abstraction:
 *     When a page is written to the disk from the coremap, it become clean. Although we could just always
 *     skip this by writing to the disk when we need to evict, this is not the most efficient. This seperation
 *     of writing to disk, and evicting lets us to later on create a thread that periodically writes pages to 
 *     disk. That means when we want to evict, we already have clean pages!
 *     So swapping is actual writing to the disk. Evicting is the process of clearing the TLB and updating 
 *     the page tables.
 * 
 * synchronization:
 *     Why do we turn off interrupts and also use locks at the same time? The answer is as follows. When 
 *     we do I/O with the swap disk, we actually need to acquire the lock for the swap file. This means that
 *     there is a possibility out prog could be put to sleep on the lock... which defeats the reason why
 *     we set interrupts off to begin with. This is why we should protect vm_fault and swap operations 
 *     with locks as well as interrupts.
 * 
 * when do we evict?
 *     This is a question of optimization. When moving a page from the swap file to the physical memory,
 *     it actually might be good to keep the page in swap disk so that we don't need to write back in the future.
 * 
 */

/* make this lock external... we now put a lock on all operations that may affect the swapping system */
struct lock;

/* 
 * So when to use this lock?
 * This is basically the only structure standing between a lot of synchronization issues
 * Use this whenever allocating ANYTHING. Basically the states between the pagetable, coremap,
 * and TLB need to be synchronized... otherwise bad things will happen
 */
extern struct lock *swap_lock;

struct pte;

/* 
 * Initialize swap disk and all its pertaining fields
 * This is called in main after vfs_bootstrap and dev_bootstrap 
 */
void swap_bootstrap();

/* Given a swapfile location, read the swap page into physical page */
int swap_read(u_int32_t swap_location, paddr_t ppage);

/* Given a swapfile locations, write the physical page into the swap location */
int swap_write(u_int32_t swap_location, paddr_t ppage);

/* 
 * Swap a page out. The page to swap out depends on a certain eviction policy
 */
int swap_pageout();

/*
 * Given a page table entry, get it back into memory.
 */
int swap_pagein(struct pte *entry);

/*
 * Given a page table entry, evict the page from the coremap
 */
void swap_pageevict(struct pte *entry);

/*
 * Use swapping to free up npages of memory
 * Return the first physical page. Return 0 if no available chain of pages was found
 * This should only be used when allocating kernel pages
 */
int swap_createspace(int npages);

/*
 * Allocate pages on demand.
 * Reserve a space in swap disk.
 */
int swap_allocpage_od(struct pte *entry);

/*
 * Use to free a page from the swap disk
 */
void swap_diskfree(u_int32_t swap_location);
int  swap_diskalloc(u_int32_t *swap_location);


#endif /* _SWAP_H_ */
