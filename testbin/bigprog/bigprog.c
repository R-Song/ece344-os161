/* bigprog.c
 *
 * Operate on a big array. The array is huge but we only operate on a part of
 * it. This is designed to test demand paging, when swapping is not implemented
 * yet.
 *
 * Kuei Sun (kuei.sun@mail.utoronto.ca)
 * David Lion (david.lion@mail.utoronto.ca)
 *
 * University of Toronto
 * 2020
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/* 20 MB array */
#define SIZE ((20 * 1024 * 1024)/sizeof(u_int32_t))

/* (david): create a struct to group the array so that we can fill the
 *          data in the right location for magic_num.
 */
struct big_struct {
        u_int32_t bigarray1[SIZE];
        u_int32_t magic_num;
        u_int32_t bigarray2[SIZE];
};

static struct big_struct big = {{0}, 344, {0}};

int
main()
{
        if (big.magic_num == 344) {
                printf("Passed bigprog test.\n");
                exit(0);
        }
        else {
                printf("bigprog test failed\n");
                exit(1);
        }
}
