
#include <types.h>
#include <pagetable.h>
#include <coremap.h>
#include <permissions.h>
#include <lib.h>
#include <synch.h>
#include <kern/errno.h>
#include <machine/vm.h>
#include <machine/spl.h>
#include <vm.h>
#include <swap.h>


/**************************************************
 * Functions that work with page table entries
 **************************************************/

/* pte_init() */
struct pte *pte_init() 
{
    struct pte *entry = kmalloc(sizeof(struct pte));
    if(entry == NULL) {
        return NULL;
    }
    entry->ppageaddr = 0;
    entry->permissions = set_permissions(0,0,0);
    entry->swap_state = PTE_NONE;
    entry->swap_location = 0;
    return entry;
}

/* pte_copy() */
void pte_copy(struct pte *src, struct pte *dest) 
{
    dest->ppageaddr = src->ppageaddr;
    dest->permissions = src->permissions;
    dest->swap_state = src->swap_state;
    dest->swap_location = src->swap_location;
}

/* pte_destroy() */
void pte_destroy(struct pte *entry) 
{
    kfree(entry);
}



/********************************************
 * Operations on page tables
 ********************************************/

#if !OPT_TWOLEVELPAGETABLE

/* initialize a new page table */
pagetable_t pt_init() 
{
    struct pte_container *head = (struct pte_container *)kmalloc(sizeof(struct pte_container));
    if(head == NULL) {
        return NULL;
    }
    head->first_idx = 0;
    head->pte_array = NULL;
    head->next = NULL;
    return head;
}

/* add entry into the page table */
int pt_add(pagetable_t pt, vaddr_t vaddr, struct pte *entry)
{
    assert(pt != NULL);
    assert( (vaddr > 0) && (vaddr < MIPS_KSEG0) );

    u_int32_t first_idx = PT_VADDR_TO_FIRST_INDEX(vaddr);
    u_int32_t second_idx = PT_VADDR_TO_SECOND_INDEX(vaddr);

    struct pte_container *head = pt;
    struct pte_container *it = pt;
    struct pte_container *tail = NULL;
    unsigned i;

    /* Loop through the linked list */
    while(1) {
        if(it->first_idx == first_idx) {
            assert(it->pte_array[second_idx] == NULL);
            it->pte_array[second_idx] = entry;
            goto pt_add_done;
        }
        if(it->next != NULL) {
            it = it->next;
        }
        else {
            break;
        }
    }

    tail = it;

    /* Didn't find a suitable entry, create a new entry */
    if(head->pte_array == NULL) {
        /* This means the page table is empty. We fill out the first entry */
        head->first_idx = first_idx;
        head->pte_array = (struct pte **)kmalloc(PT_PTE_ARRAY_NUM_ENTRIES * sizeof(struct pte *));
        if(head->pte_array == NULL) {
            return ENOMEM;          /* If add fails, pt will be destroyed by as_destroy */
        } 
        for(i=0; i<PT_PTE_ARRAY_NUM_ENTRIES; i++) {
            head->pte_array[i] = NULL;
        }

        head->pte_array[second_idx] = entry;
        assert(head->next == NULL);
        goto pt_add_done;
    }
    else {
        /* Allocate for the next one at the end of the tail */
        assert(tail->next == NULL);

        /* Allocate the pte_container */
        tail->next = (struct pte_container *)kmalloc( sizeof(struct pte_container) );

        /* Set the first idx and allocate space for the array */
        tail->next->first_idx = first_idx;
        tail->next->pte_array = (struct pte **)kmalloc(PT_PTE_ARRAY_NUM_ENTRIES * sizeof(struct pte *));
        if(tail->next->pte_array == NULL) {
            return ENOMEM;          /* If add fails, pt will be destroyed by as_destroy */
        }
        for(i=0; i<PT_PTE_ARRAY_NUM_ENTRIES; i++) {
            tail->next->pte_array[i] = NULL;
        }

        /* Add the entry into the pte_array and set the next pointer to NULL */
        tail->next->pte_array[second_idx] = entry;
        tail->next->next = NULL;
        goto pt_add_done;
    }

pt_add_done:
    return 0;
}

/* get pte entry */
struct pte *pt_get(pagetable_t pt, vaddr_t vaddr)
{
    assert(pt != NULL && pt->pte_array != NULL);

    u_int32_t first_idx = PT_VADDR_TO_FIRST_INDEX(vaddr);
    u_int32_t second_idx = PT_VADDR_TO_SECOND_INDEX(vaddr);

    struct pte_container *it = pt;
    struct pte *entry;

    while(1) {
        if(it->first_idx == first_idx) {
            entry = it->pte_array[second_idx];
            return entry;
        }
        if(it->next != NULL) {
            it = it->next;
        }
        else {
            break;
        }
    }

    return NULL;
}


/* 
 * get the next populated vaddr 
 * Note, get next does not get the next vaddr in order! it just simply the order of the linked list...
 * If someone calls pt_getnext(pt, 0), we return the first vaddr in the linked list...
 */
vaddr_t pt_getnext(pagetable_t pt, vaddr_t vaddr)
{
    assert(pt != NULL && pt->pte_array != NULL);

    u_int32_t first_idx = PT_VADDR_TO_FIRST_INDEX(vaddr);
    u_int32_t second_idx = PT_VADDR_TO_SECOND_INDEX(vaddr);

    struct pte_container *it = pt;
    unsigned i;

    /* Return first valid entry in page table */
    if(vaddr == 0) {
        for(i=0; i<PT_PTE_ARRAY_NUM_ENTRIES; i++) {
            if(pt->pte_array[i] != NULL) {
                return PT_INDEX_TO_VADDR(pt->first_idx, i);
            }
        }
    }

    /* Get to the first current vaddr */
    while(1) {
        if(it->first_idx == first_idx) {
            break;
        }
        else if(it->next != NULL) {
            it = it->next;
        }
        else {
            return 0; /* Could not find vaddr */
        }
    }

    /* Return the next one */
    while(1) {
        if(it->first_idx == first_idx) {
            for(i=second_idx+1; i<PT_PTE_ARRAY_NUM_ENTRIES; i++) {
                if(it->pte_array[i] != NULL) {
                    return PT_INDEX_TO_VADDR(it->first_idx, i);
                }
            }
        }
        else {
            for(i=0; i<PT_PTE_ARRAY_NUM_ENTRIES; i++) {
                if(it->pte_array[i] != NULL) {
                    return PT_INDEX_TO_VADDR(it->first_idx, i);
                }
            }
        }
        if(it->next != NULL) {
            it = it->next;
        }
        else {
            break;
        }
    }

    return 0;
}

/* 
 * copy a page table
 * This is only meant to be called to copy pages to a fresh page table 
 */
int pt_copy(pagetable_t src, pagetable_t dest)
{
    assert(src != NULL && src->pte_array != NULL && dest != NULL);
    unsigned i;

    int spl = splhigh(); /* Working with two page tables, lets be careful */

    while(1) {
        dest->first_idx = src->first_idx;
        assert(dest->pte_array == NULL);
        dest->pte_array = (struct pte **)kmalloc(PT_PTE_ARRAY_NUM_ENTRIES * sizeof(struct pte *));
        if(dest->pte_array == NULL) {
            splx(spl);
            return ENOMEM;
        }

        for(i=0; i<PT_PTE_ARRAY_NUM_ENTRIES; i++) {
            if(src->pte_array[i] != NULL){

                dest->pte_array[i] = pte_init();
                if(dest->pte_array[i] == NULL) {
                    splx(spl);
                    return ENOMEM;
                }

                pte_copy(src->pte_array[i], dest->pte_array[i]);
            }
            else {
                dest->pte_array[i] = NULL;
            }
        }

        if(src->next != NULL) {
            src = src->next;

            assert(dest->next == NULL);

            dest->next = (struct pte_container *)kmalloc(sizeof(struct pte_container));
            if(dest->next == NULL) {
                splx(spl);
                return ENOMEM;
            }
            
            dest->next->first_idx = 0;
            dest->next->pte_array = NULL;
            dest->next->next = NULL;

            dest = dest->next;
        }
        else {
            break;
        }
    }

    splx(spl);
    return 0;
}


/* remove entry from the page table */
void pt_remove(pagetable_t pt, vaddr_t vaddr)
{
    assert(pt != NULL && pt->pte_array != NULL);

    u_int32_t first_idx = PT_VADDR_TO_FIRST_INDEX(vaddr);
    u_int32_t second_idx = PT_VADDR_TO_SECOND_INDEX(vaddr);

    struct pte_container *it = pt;

    while(1) {
        if(it->first_idx == first_idx) {
            it->pte_array[second_idx] = NULL;
            return;
        }
        if(it->next != NULL) {
            it = it->next;
        }
        else {
            return;
        }
    }
}


/* 
 * destroy page table 
 * This is such an awful way to destroy everything. But I'm pressed on time :/
 */
void pt_destroy(pagetable_t pt)
{
    struct pte_container *it;
    struct pte_container *head;
    unsigned i;
    assert( lock_do_i_hold(swap_lock) );
    
    head = pt;

    while(1) {
        it = head;
        while(1) {
            if(it->next == NULL) {
                break;
            }

            if(it->next->next == NULL) {
                /* destroy the last pte_container */
                for(i=0; i<PT_PTE_ARRAY_NUM_ENTRIES; i++) {
                    if(it->next->pte_array[i] != NULL) {
                        if(it->next->pte_array[i]->ppageaddr != 0) {
                            free_upage(it->next->pte_array[i]);
                        }
                        pte_destroy(it->next->pte_array[i]);
                    }
                }
                kfree(it->next->pte_array);
                kfree(it->next);
                it->next = NULL;
                break;
            }
            else {
                it = it->next;
            }
        }
        if(it == head) {
            if(head->pte_array != NULL) {
                for(i=0; i<PT_PTE_ARRAY_NUM_ENTRIES; i++) {
                    if(head->pte_array[i] != NULL) {
                        if(head->pte_array[i]->ppageaddr != 0) {
                            free_upage(head->pte_array[i]);
                        }
                        pte_destroy(head->pte_array[i]);
                    }
                }
                kfree(head->pte_array);
            }
            kfree(head);
            return;
        }
    }
}


/* dump all contents of pagetable to console */
void pt_dump(pagetable_t pt)
{
    struct pte_container *it;
    unsigned i;
    vaddr_t vaddr;

    it = pt;

    while(1) {
        if(it != NULL && it->pte_array != NULL) {
            for(i=0; i<PT_PTE_ARRAY_NUM_ENTRIES; i++) {
                vaddr = PT_INDEX_TO_VADDR(it->first_idx, i);
                kprintf("Vaddr: 0x%08x  |  Paddr: 0x%08x  |  Permissions: %d\n", 
                        vaddr, it->pte_array[i]->ppageaddr, it->pte_array[i]->permissions);
            }
            it = it->next;
        }
        else {
            break;
        }
    }

}







#else

/******************************************************************************************************************************/
/******************************************************************************************************************************/
/******************************************************************************************************************************/
/******************************************************************************************************************************/
/******************************************************************************************************************************/
/******************************************************************************************************************************/
/******************************************************************************************************************************/
/******************************************************************************************************************************/
/******************************************************************************************************************************/
/******************************************************************************************************************************/

/*
 * This is the two level page table implementation:
 * Although faster, we shouldn't be using this anymore as it simply takes up too much memory!
 */

/* Machine dependant stuff */
u_int32_t pt_vaddr_to_first_index(vaddr_t addr) {
    int idx = ((addr >> PAGE_OFFSET) >> PT_SECOND_LAYER_OFFSET);
    return idx;
}
u_int32_t pt_vaddr_to_second_index(vaddr_t addr) {
    int idx = ((addr >> PAGE_OFFSET) & PT_SECOND_LAYER_MASK);
    return idx;
}
vaddr_t idx_to_vaddr(u_int32_t first_idx, u_int32_t second_idx) {
    return ( ((first_idx << (PAGE_OFFSET)) << PT_SECOND_LAYER_OFFSET) | (second_idx << PAGE_OFFSET) );
}


/* pt_init() */
pagetable_t pt_init() 
{
    unsigned i;
    pagetable_t pt;

    pt = (pagetable_t)kmalloc(PT_FIRST_LAYER_SIZE * sizeof(struct pte **));
    if(pt == NULL) {
        return NULL;
    }

    for(i=0; i<PT_FIRST_LAYER_SIZE; i++) {
        pt[i] = NULL;
    }
    return pt;
}


/* 
 * pt_add() 
 * Create second layer tables on demand. Return non-zero value on error
 * We don't allocate memory for a pte here, do that before you call pt_add
 */
int pt_add(pagetable_t pt, vaddr_t addr, struct pte *entry) 
{
    unsigned i;
    u_int32_t first_layer_idx = pt_vaddr_to_first_index(addr);
    u_int32_t second_layer_idx = pt_vaddr_to_second_index(addr);
    
    if(pt[first_layer_idx] == NULL) {
        pt[first_layer_idx] = (struct pte **)kmalloc(PT_SECOND_LAYER_SIZE * sizeof(struct pte *));
        if(pt[first_layer_idx] == NULL) {
            return ENOMEM;
        }
        for(i=0; i<PT_SECOND_LAYER_SIZE; i++) {
            pt[first_layer_idx][i] = NULL;
        }
    }

    pt[first_layer_idx][second_layer_idx] = entry;
    return 0;
}


/*
 * pt_get()
 * Get a pagetable entry
 */
struct pte *pt_get(pagetable_t pt, vaddr_t addr) {
    u_int32_t first_layer_idx = pt_vaddr_to_first_index(addr);
    u_int32_t second_layer_idx = pt_vaddr_to_second_index(addr);

    if(pt[first_layer_idx] == NULL) {
        return NULL;
    }
    return pt[first_layer_idx][second_layer_idx];
}


/*
 * pt_getnext()
 * Get the next populated vaddr table entry
 */
vaddr_t pt_getnext(pagetable_t pt, vaddr_t addr) {

    u_int32_t first_layer_idx = pt_vaddr_to_first_index(addr);
    u_int32_t second_layer_idx = pt_vaddr_to_second_index(addr);

    unsigned i, j;
    if(pt[first_layer_idx] != NULL) {
        for(j=second_layer_idx+1; j<PT_SECOND_LAYER_SIZE; j++) {
            if(pt[first_layer_idx][j] != NULL)
                return idx_to_vaddr(first_layer_idx, j);
        }
    }

    for(i=first_layer_idx+1; i<PT_FIRST_LAYER_SIZE; i++) {
        if(pt[i] == NULL)
            continue;
        else {
            for(j=0; j<PT_SECOND_LAYER_SIZE; j++) {
                if(pt[i][j] == NULL)
                    continue;
                else {
                    return idx_to_vaddr(i, j);
                }
            }
        }
    }

    return 0;
}


/*
 * pt_copy()
 * Deep copies and entire page table, including its entries.
 * Notice that the mappings will be exactly the same. It is up to the rest of the VM system to update them
 * Returns on error.
 */
int pt_copy(pagetable_t src, pagetable_t dest) {
    unsigned i, j;
    struct pte *entry;
    for(i=0; i<PT_FIRST_LAYER_SIZE; i++) {
        if(src[i] == NULL)
            continue;
        else {
            for(j=0; j<PT_SECOND_LAYER_SIZE; j++) {
                if(src[i][j] == NULL)
                    continue;
                else {
                    entry = pte_init();
                    if(entry == NULL) {
                        return ENOMEM;
                    }
                    /* Copy the pte and then add it to the new page table */
                    pte_copy(src[i][j], entry);
                    pt_add(dest, idx_to_vaddr(i, j) ,entry);
                    dest[i][j] = entry;
                }
            }
        }   
    }
    return 0;
}

/*
 * pt_remove()
 * Remove a pagetable entry
 */
void pt_remove(pagetable_t pt, vaddr_t addr) {
    u_int32_t first_layer_idx = pt_vaddr_to_first_index(addr);
    u_int32_t second_layer_idx = pt_vaddr_to_second_index(addr); 

    if(pt[first_layer_idx] == NULL || pt[first_layer_idx][second_layer_idx] == NULL) {
        return;
    }
    
    pt[first_layer_idx][second_layer_idx] = NULL;
}


/*
 * pt_destroy()
 * Destroy page table as well all its entries
 * This needs to be changed to deallocate the pages in the page table! 
 * It gets complicated for copy on write... like how do u know when to destroy a page...
 * 
 * right now just destroy all the pages, no copy on write right now
 */
void pt_destroy(pagetable_t pt) {
    unsigned i, j;
    /* Deallocate everything */
    for(i=0; i<PT_FIRST_LAYER_SIZE; i++) {
        if(pt[i] == NULL)
            continue;
        else {
            for(j=0; j<PT_SECOND_LAYER_SIZE; j++) {
                if(pt[i][j] != NULL) {
                    /* Deallocate the pages */
                    if(pt[i][j]->ppageaddr != 0){
                        free_ppages(pt[i][j]->ppageaddr);   /* Free the physical page mapping */
                    }
                    /* Deallocate the pte */
                    pte_destroy(pt[i][j]);
                }
            }
            /* Deallocate layer 2 table */
            kfree(pt[i]);
        }
    }
    /* Deallocate layer 1 table */
    kfree(pt);
}

/* Dump all contents of page table to console */
void pt_dump(pagetable_t pt)
{
    unsigned i, j;
    vaddr_t vaddr;
    kprintf("\n");
    for(i=0; i<PT_FIRST_LAYER_SIZE; i++) {
        if(pt[i] == NULL)
            continue;
        else {
            for(j=0; j<PT_SECOND_LAYER_SIZE; j++) {
                if(pt[i][j] == NULL)
                    continue;
                else {
                    vaddr = idx_to_vaddr(i, j);
                    kprintf("Vaddr: 0x%08x  |  Paddr: 0x%08x  |  Permissions: %d\n", vaddr, pt[i][j]->ppageaddr, pt[i][j]->permissions);
                }
            }
        }
    }   
    kprintf("\n");
}


#endif /* OPT_TWOLEVELPAGETABLE */
