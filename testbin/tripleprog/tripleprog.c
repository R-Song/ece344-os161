/*
 * tripleprog.c
 *
 * Calls three copies of a program. By default calls /testbin/huge
 *
 * Kuei Sun <kuei.sun@utoronto.ca>
 *
 * University of Toronto, 2017
 */

#include "triple.h"
#include <stdio.h>
#include <unistd.h>

static 
int
usage(void)
{
        printf("usage: tripleprog PROG\n");
        _exit(-1);
}

int
main(int argc, const char * argv[])
{
        const char * prog = "/testbin/huge";

        if (argc == 2) {
                prog = argv[1];
        }
        else if (argc > 2) {
                usage();
        }   

        triple(prog);
        return 0;
}

