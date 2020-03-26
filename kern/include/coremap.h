#ifndef _COREMAP_H_
#define _COREMAP_H_

/* 
 * Definition of the coremap, fundamental structure in the VM system.
 * 
 * Coremap is basically the structure that maps virtual page numbers to physical pages that are physically in memory. 
 * It is essentially a hash table with the property that there can never be more virtual page numbers than physical 
 * page numbers on the coremap!
 * 
 */

struct coremap_entry {
    /* Have to keep track of who owns this page */
    struct thread* owner;



};





/*
 *
 */


#endif /* _COREMAP_H_ */