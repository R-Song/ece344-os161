/* btree.c
 *
 * Operate on a stack or heap region buffer. The buffer is big but we only 
 * operate on just the front and the back of it. This is designed to test 
 * demand paging for the stack and heap, when swapping is not implemented yet.
 *
 * Note: use bigprog to test demand paging on the data region
 *
 * Kuei Sun <kuei.sun@utoronto.ca>
 *
 * University of Toronto, 2013
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 1 MB array */
#define SIZE (1024 * 1024)

/* We'll copy in at least 32K of data into the btree */
#define MIN (32 * 1024)

static const char * value[] = {
    "blue",
    "red",
    "yellow",
    "green",
    "gray",
    "black",
    "white",
    "cyan",
    "magenta",
    "orange",
    "pink",
    "purple",
    "violet",
    "teal",
};

#define N (sizeof(value)/sizeof(const char *))

struct btree_item
{
    unsigned key;
    unsigned nbytes;
    unsigned offset;  
};

struct btree_leaf
{
    unsigned len;
    unsigned ptr;
    struct btree_item item[];
};

static
int 
insert(struct btree_leaf * leaf, unsigned key, const char * val)
{
    int i = leaf->len;
    char * buf = (char *)leaf;
    size_t size;
    unsigned offset;
    
    leaf->item[i].key = key;
    leaf->item[i].nbytes = strlen(val) + 1;
    offset = leaf->ptr - leaf->item[i].nbytes;
    size = sizeof(unsigned)*2 + sizeof(struct btree_item)*(i+1);
    
    if ( size < offset ) {
        leaf->item[i].offset = offset;
        strcpy(buf + offset, val);
        leaf->len++;
        leaf->ptr = offset;
        return leaf->item[i].nbytes;
    } 
    return 0;
}

static
int
btree(char * buf)
{
    unsigned size = 0;
    unsigned k = 0;
    struct btree_leaf * leaf = (struct btree_leaf *)buf;
    leaf->len = 0;
    leaf->ptr = SIZE;
    
    while ( size < MIN ) {
        unsigned len;
        k++;
        len = insert(leaf, k, value[k%N]);
        if ( len == 0 )
            break;
        size += len;
    }
    
    for ( k = 1; k <= leaf->len; k++ ) {
        unsigned key = leaf->item[k-1].key;
        char * val = buf + leaf->item[k-1].offset;  
        if ( key != k ) {
            printf("btree test failed\n");
            return 1;
        }
        if ( strcmp(val, value[k%N]) ) {
            printf("btree test failed\n");
            return 1;      
        }
    }
    
    printf("Passed btree test.\n");
    return 0;
}

static
void
heap()
{
    char * buf = (char *)malloc(sizeof(char)*SIZE);
    if ( buf == NULL ) {
        printf("malloc failed\n");
        exit(1);
    }
    exit(btree(buf));
}

static
void
stack()
{
    char buf[SIZE];
    exit(btree(buf));
}


int
main(int argc, const char * argv[])
{
    if ( argc == 2 ) {
        if ( strcmp(argv[1], "-s") == 0 ) {
            stack();
        } 
        else if ( strcmp(argv[1], "-h") == 0 ) {
            heap();
        }
    } else if ( argc == 0 || argc == 1 ) {
        stack();
    }
    
    printf("usage: %s [-s|-h]\n"
           "   -s      test the stack\n"
           "   -h      test the heap\n"
           "   --help  display this help message\n", argv[0]);
    return 1;
}

