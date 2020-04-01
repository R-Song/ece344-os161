
#include <types.h>
#include <pagetable.h>
#include <lib.h>
#include <kern/errno.h>
#include <machine/vm.h>


/* Machine dependant stuff */
int pt_vaddr_to_first_index(vaddr_t addr) {
    int idx = ((addr >> PAGE_OFFSET) >> PT_SECOND_LAYER_OFFSET);
    return idx;
}
int pt_vaddr_to_second_index(vaddr_t addr) {
    int idx = ((addr >> PAGE_OFFSET) & PT_SECOND_LAYER_MASK);
    return idx;
}
vaddr_t idx_to_vaddr(int first_idx, int second_idx) {
    return ( (first_idx << (PAGE_OFFSET+10)) | (second_idx << PAGE_OFFSET) );
}


/* pt_init() */
pagetable_t pt_init() 
{
    int i;
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
 */
int pt_add(pagetable_t pt, vaddr_t addr, struct pte *entry) 
{
    int i;
    int first_layer_idx = pt_vaddr_to_first_index(addr);
    int second_layer_idx = pt_vaddr_to_second_index(addr);
    
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
    int first_layer_idx = pt_vaddr_to_first_index(addr);
    int second_layer_idx = pt_vaddr_to_second_index(addr);

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

    int first_layer_idx = pt_vaddr_to_first_index(addr);
    int second_layer_idx = pt_vaddr_to_second_index(addr);

    int i, j;
    if(pt[first_layer_idx] != NULL) {
        for(j=second_layer_idx+1; j<PT_SECOND_LAYER_SIZE; j++) {
            if(pt[first_layer_idx][i] != NULL)
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
    int i, j;
    struct pte *entry;
    for(i=0; i<PT_FIRST_LAYER_SIZE; i++) {
        if(src[i] == NULL)
            continue;
        else {
            for(j=0; j<PT_SECOND_LAYER_SIZE; j++) {
                if(src[i][j] == NULL)
                    continue;
                else {
                    entry = kmalloc(sizeof(struct pte));
                    if(entry == NULL) {
                        return ENOMEM;
                    }
                    dest[i][j] = entry;
                    /* Copy over the pte fields */
                    dest[i][j]->ppage = src[i][j]->ppage;
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
    int first_layer_idx = pt_vaddr_to_first_index(addr);
    int second_layer_idx = pt_vaddr_to_second_index(addr); 

    if(pt[first_layer_idx] == NULL || pt[first_layer_idx][second_layer_idx] == NULL) {
        return;
    }

    kfree(pt[first_layer_idx][second_layer_idx]);
    pt[first_layer_idx][second_layer_idx] = NULL;
}


/*
 * pt_destroy()
 * Destroy page table as well all its entries
 */
void pt_destroy(pagetable_t pt) {
    int i, j;
    /* Deallocate everything */
    for(i=0; i<PT_FIRST_LAYER_SIZE; i++) {
        if(pt[i] == NULL)
            continue;
        else {
            for(j=0; j<PT_SECOND_LAYER_SIZE; j++) {
                if(pt[i][j] != NULL) {
                    kfree(pt[i][j]);
                }
            }
            kfree(pt[i]);
        }
    }
    kfree(pt);
}


