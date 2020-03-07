/*
 * Hashmap_uh. See hashmap.h for details.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <hashmap.h>

struct hashmap {
    int size;
    struct h_entry *map; /* array entries */
}

struct h_entry {
    int key;
    void *data_block;
    
}

