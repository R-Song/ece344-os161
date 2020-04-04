
/*
 * Defintion of the pagetable
 *  
 * The purpose of a page table is to translate virtual addresses to physical addresses.
 * Each addrspace should have its own page table. This implements a two level page table.
 * 
 * In this specific case, there are 2^19 user level pages that we need to potentially provide a mapping for.
 * The first page table will have 2^9 (512) entries, each with a pointer to another table with 2^10 (1024) entries.
 * This covers all 19 bits we need to describe the virtual page number. The second layer of tables is created on demand.
 * 
 * Each of the entries in one of the tables with 512 entries is a page table entry, with a ton of useful information about the page.
 * That is also defined in this file
 * 
 */

#ifndef _PAGETABLE_H_
#define _PAGETABLE_H_

#include <permissions.h>

/*
 * Machine dependant stuff
 * Hardcoded sizes of the first and second layer. Really depends on the machine.
 * Here, the first and second layer cover 19 bits, which is enough to index from MIPS_KUSEG to MIPS_KSEG0
 */
#define PT_FIRST_LAYER_SIZE 512
#define PT_SECOND_LAYER_SIZE 1024
#define PT_SECOND_LAYER_OFFSET 10
#define PT_SECOND_LAYER_MASK 0x000007ff     /* bottom 10 bits */

int pt_vaddr_to_first_index(vaddr_t addr);
int pt_vaddr_to_second_index(vaddr_t addr);
vaddr_t idx_to_vaddr(int first_idx, int second_idx);

/*
 * page table entry (pte)
 * 
 * right now just has physical mapping, but will need more fields later...
 */
struct pte {
	paddr_t ppageaddr;
    /* Tentative fields to add to support copy on write */
    struct semaphore *pte_mutex; /* Mutual exclusion for this page */
    int num_users;               /* How many users are reading from this page? */
    int dirty;                   /* Is this page safe to write to */
    
    /* permissions */
    permissions_t permissions;
};

/* create and destroy a pte */
struct pte *pte_init();
void pte_destroy(struct pte *entry); 

/* Copy a pte entry */
void pte_copy(struct pte *src, struct pte *dest);


/* pagetable definition */
typedef struct pte*** pagetable_t;     


/* initialize a new page table */
pagetable_t pt_init();

/* add entry into the page table */
int pt_add(pagetable_t pt, vaddr_t addr, struct pte *entry);

/* get pte entry */
struct pte *pt_get(pagetable_t pt, vaddr_t vaddr);

/* get the next populated vaddr */
vaddr_t pt_getnext(pagetable_t pt, vaddr_t addr);

/* copy a page table */
int pt_copy(pagetable_t src, pagetable_t dest);

/* remove entry from the page table */
void pt_remove(pagetable_t pt, vaddr_t vaddr);

/* destroy page table */
void pt_destroy(pagetable_t pt);


#endif /* _PAGETABLE_H_ */
