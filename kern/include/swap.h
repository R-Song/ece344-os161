#ifndef _SWAP_H_
#define _SWAP_H_

/*
 * Implementation of the page swap system. Includes a few parts:
 *     1) Data structure to locate specific pages on the swap file
 *     2) Interface with the swap file to allow read and writes to swapfile pages
 *     3) Interface to swap pages in and out of the coremap, keeping pagetables and coremap updated
 *     4) Method to make pages available for the kernel on large allocations
 * 
 * A few notes:
 *     When a page is written to the disk from the coremap, it become clean. Although we could just always
 *     skip this by writing to the disk when we need to evict, this is not the most efficient. This seperation
 *     of writing to disk, and evicting lets us to later on create a thread that periodically writes pages to 
 *     disk. That means when we want to evict, we already have clean pages!
 *     So swapping is actual writing to the disk. Evicting is the process of clearing the TLB and updating 
 *     the page tables.
 * 
 */





#endif /* _SWAP_H_ */