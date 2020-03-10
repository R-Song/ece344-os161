/*
 * Hashmap_uh. See hashmap.h for details.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <hashmap.h>

/*
 * Hash Map data structure implemented with a linked list 
 */ 
struct hashmap {
    int size;
    struct h_entry **map; /* array entries */
};

/*
 * Data items that are inserted in the hash map
 */
struct h_entry {
    int key; 
    void *data_block;
    struct h_entry *previous;
    struct h_entry *next;
};

/* h_create(size): creates a hashmap with 'size' entries. Hashing function will be (key % size)
 *                 if collisions occur, a linked list will be built off of each hashmap entry  
 */
struct hashmap *h_create(int size)
{
    /* Allocating space for new hashmap */
    struct hashmap *new_hashmap = (struct hashmap*)kmalloc(sizeof(struct hashmap));
    new_hashmap->size = size;
    new_hashmap->map = (struct h_entry**)kmalloc( size * sizeof(struct h_entry*));

    /* Set every hash table entry pointer to NULL */
    int i;
    for(i = 0; i < size; i++){
        new_hashmap->map[i] = NULL;    
    }

    return new_hashmap;
}

/*
 * h_getentry(key): returns the hashmap entry. returns NULL if entry doesnt exist (search)
 */
struct h_entry *h_getentry(int key, struct hashmap *h_map)
{
    int size = h_map->size;
    /* Acquire that guap aka hash index based on the key */
    int hash_index = h_function(key, size);

    struct h_entry *curr_entry = h_map->map[hash_index];  

    /* Search in the linked list at given hash index in the table until key is found or NULL point is reached*/
    while ( curr_entry != NULL && curr_entry->key != key ){
        curr_entry = curr_entry->next;  
    }
    /* No entry with given key found */
    if( curr_entry == NULL ){
        return NULL;
    }

    return curr_entry;
}

/*
 * h_function(int key, int size): returns the hash index based on the given key
 */
int h_function(int key, int size)
{
    // do we need to handle a negative key??
    return key % size;    
}

/*
 * h_insert(key): returns the 1 of the entry if entry was inserted successfully, returns 0 if unsuccessful  
 */
int h_insert(int key, struct h_entry *h_entry, struct hashmap *h_map)
{
    /* If the entry is NULL or the hash map is empty, return -1 */
    if ( h_entry == NULL || h_map == NULL ){
        return 0;
    }

    int size = h_map->size;
    /* Get the hash index based on the key */
    int hash_index = h_function(key, size);

    struct h_entry *curr_entry = h_map->map[hash_index];
    /* Inserting the first element in the linked list at the specific hash index */
    if ( curr_entry == NULL ){
        curr_entry = h_entry;
        return 1;
    }

    /* Find the first empty entry slot at the given hash index */
    while ( curr_entry != NULL ){
        curr_entry = curr_entry->next;
    } 

    /* Add the new entry in the linked list at the given hash index*/
    curr_entry->previous = curr_entry;
    curr_entry->next = h_entry;

    return 1;
}

/*
 * h_getavail(key): returns 1 if key is available, returns 0 if key is not available
 */
int h_keyavail(int key, struct hashmap *h_map)
{
    int size = h_map->size;
    /* Get the hash index based on the key */
    int hash_index = h_function(key, size);  

    struct h_entry *curr_entry = h_map->map[hash_index]; 

    /* No entries at the given hash index */
    if( curr_entry == NULL){
        return 0;
    }

    while(curr_entry != NULL && curr_entry->key != key){
        curr_entry = curr_entry->next;
    }

    /* Key already exists in the table, therefore it is not available*/
    if(curr_entry->key == key){
        return 0;
    }

    /* Key does not exist in the linked list, therefore it is available*/
    if(curr_entry != NULL){
        return 1;
    }
    /* If NULL point was reached and no key matched, then key is available*/
    return 1;
}

/*
 * h_rementry(int key): returns 1 if entry was removed successfully, returns 0 if unsuccessful. Possible that the entry didn't exist.
 */
int h_rementry(int key, struct hashmap *h_map)
{
    struct h_entry *lookup_entry = h_getentry(key, h_map);

    /* Entry does not exist */
    if( lookup_entry == NULL ){
        return 0;
    }  
    /* Reattach the next link */
    struct h_entry *prev = lookup_entry->previous;
    prev->next = lookup_entry->next;
    
    lookup_entry->next = NULL;
    lookup_entry->previous = NULL;
    /* Free data block pointer */
    kfree(lookup_entry->data_block);
    lookup_entry->data_block = NULL;
    
    kfree(lookup_entry);
    lookup_entry = NULL;
    
    return 1;
}

/*
 * h_destroy(struct hashmap *): deallocates entire hashmap
 */
void h_destroy(struct hashmap *h_map)
{
    int size = h_map->size;
    int i;
    struct h_entry *curr_entry;
    int key_to_remove;

    for( i = 0; i < size; i++ ){
        /* There are no elements in the linked list at this specific index*/
        if (h_map->map[i] == NULL){
            break;
        } 
        curr_entry = h_map->map[i];
        /* Search for the last non-NULL entry in the linked list */
        while(curr_entry->next != NULL){
            key_to_remove = curr_entry->key;
            curr_entry = curr_entry->next;
            h_rementry(key_to_remove, h_map);
        }
        /* Remove the last element in the linked list */
        h_rementry(curr_entry->key, h_map);

        kfree(h_map->map[i]);
        h_map->map[i] = NULL;
    }

    kfree(h_map->map);
    h_map->map = NULL;
}







