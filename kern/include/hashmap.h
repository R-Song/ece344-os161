#ifndef _HASHMAP_H_
#define _HASHMAP_H_

/* 
 * Basic Hashmap_uh:
 * 
 * h_create(size): creates a hashmap with 'size' entries. Hashing function will be (key % size)
 *                 if collisions occur, a linked list will be built off of each hashmap entry  
 * 
 * h_getentry(key): returns the hashmap entry. returns NULL if entry doesnt exist
 * 
 * h_getavail(key): returns 1 if key is available, returns 0 if key is not available
 * 
 * h_insert(key): returns 1 if entry was inserted successfully, returns 0 if unsuccessful  
 * 
 * h_rementry(int key): returns 1 if entry was removed successfully, returns 0 if unsuccessful. Possible that the entry didn't exist.
 * 
 * h_destroy(struct hashmap *): deallocates entire hashmap
 * 
 * h_function(int key, int size): returns the hash index based on the given key
 */

struct hashmap;
struct h_entry;

struct hashmap *h_create(int size);
struct h_entry *h_getentry(int key, struct hashmap *h_map);
int             h_keyavail(int key, struct hashmap *h_map);
int             h_insert(int key, struct h_entry *h_entry, struct hashmap *h_map);
int             h_rementry(int key, struct hashmap *h_map);
void            h_destroy(struct hashmap *h_map);
int             h_function(int key, int size);

#endif /* _HASHMAP_H_ */
