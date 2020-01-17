/* 
 * hasher.c
 *
 * forks the specified number of chilren and then have each of them 
 * calculate the hash of a readonly dataset multiple times. This test 
 * verifies that copy-on-write is working. If it is not working, 
 * then the parent process would not be able to finish creating all
 * child processes before some of the child processes finish.
 *
 * Kuei Sun <kuei.sun@utoronto.ca>
 *
 * University of Toronto, 2016
 *
 */

#include "say.h"
#include <err.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#define PAGE_SIZE    4096
#define NUM_PAGES    12
#define SIZE         (NUM_PAGES*PAGE_SIZE)

/* 12 pages of random latin */
char document[SIZE];

#define INT_PAGE    (PAGE_SIZE/sizeof(int))
#define INT_SIZE    (SIZE/sizeof(int))

int hash_document(int index)
{
    int i, j;
    int hash = 0;
        
    i = index;
    do {
        for (j = 0; j < NUM_PAGES; ++j) {
            /* page-width stride */
            hash ^= ((int *)document)[i+j*INT_PAGE];
        }
        
        i = (i+1) % INT_PAGE;
    } while (i != index);
    
    return hash;
}

#define ANSWER 0x3f5a255b

int hasher(int index, int runtime)
{ 
    time_t before, now;
    
    before = time(NULL);
 
    do {
        int hash = hash_document(index);
        
        if (hash != ANSWER) {
            say("hasher[%d]: failed. incorrect hash value %p\n", index,
                (void *)hash);
            return 0;
        }
        
        now = time(NULL);
    } while (now - before <= runtime);
    
    say("hasher[%d]: pid %d done at t=%d\n", index, getpid(), now);
    return now;
}

const char * text[] = {
    "Lorem ipsum dolor sit amet",
    
    "Consectetur adipiscing elit",
    
    "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua",
  
    "Ut enim ad minim veniam",
    
    "Quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
    "commodo consequat",

    "Duis aute irure dolor in reprehenderit in voluptate velit esse "
    "cillum dolore eu fugiat nulla pariatur",
    
    "Excepteur sint occaecat cupidatat non proident",
    
    "Sunt in culpa qui officia deserunt mollit anim id est laborum",
    
    "At vero eos et accusamus et iusto odio dignissimos ducimus",
    
    "Temporibus autem quibusdam et aut officiis debitis aut rerum"
    " necessitatibus saepe eveniet",
    
    "Itaque earum rerum hic tenetur a sapiente delectus",
};

/* deterministic pseudo-random */
int drand(void)
{
    static int seed = 123456789;
    seed = 1103515245 * seed + 12345;
    return seed;  
}

#define tolower(c) ((c)+('a'-'A'))

#define NUM_TEXTS (sizeof(text)/sizeof(const char *))

void fill(void)
{
    char * cptr = (char *)document;
    int size = (int)sizeof(document);
    int lower = 0;

    while (size > 0) {
        int len;
        int next;
        
        if (drand()%2) {
            len = snprintf(cptr, size, "%s, ", text[drand()%NUM_TEXTS]);
            next = 1;
        }
        else {
            len = snprintf(cptr, size, "%s. ", text[drand()%NUM_TEXTS]);
            next = 0;
        }
        
        if (lower)
            cptr[0] = tolower(cptr[0]);
            
        cptr += len;
        size -= len;
        lower = next;
    }
    
    assert(document[SIZE-1] == '\0');
    document[SIZE-2] = '.';
}

void fill_deadbeef(int nints, int * buf)
{
    int i;
    
    for (i = 0; i < nints; ++i) {
        buf[i] = 0xdeadbeef;
    }
}

#define DEFAULT_RUNTIME  4
#define DEFAULT_NPIDS    16

void usage(const char * metavar) {
    if (metavar)
        say("hasher: %s must be greater than zero\n", metavar);
    say("usage: hash [NUM=%d][TIME=%d]\n", DEFAULT_NPIDS,
           DEFAULT_RUNTIME);
    _exit(-1);
}

#define MAX_NPIDS 32

int main(int argc, const char * argv[])
{
    int i = 0;
    int ret = -1;
    pid_t pids[MAX_NPIDS];
    time_t start;
    int pass = 1;
    int npids = DEFAULT_NPIDS;
    int runtime = DEFAULT_RUNTIME;
    
    if (argc >= 2) {
        npids = atoi(argv[1]);
        
        if (npids >= MAX_NPIDS)
            npids = MAX_NPIDS;
        else if (npids <= 0)
            usage("NUM");
            
        if (argc == 3) {
            runtime = atoi(argv[2]);  
            if (runtime <= 0)
                usage("TIME");
        }
        else if (argc > 3)
            usage(NULL);
    }
    
    fill();
    say("hasher: spawning %d child process(es)\n", npids);

    while (i < npids) {
        ret = fork();
        if (ret > 0)
            pids[i++] = ret;
        else 
            break;
    }
    
    /* child process - do work */
    if (ret == 0) {      
        return hasher(i, runtime);
    }
    else if (ret < 0) {
        warn("fork");
        return -1;
    }

    start = time(NULL);
    say("hasher: created %d child process(es)\n", i);
    say("hasher: running for %d seconds\n", runtime); 
    
    /* blow up some data to make sure copy-on-write is working */
    fill_deadbeef(INT_PAGE, (int *)document);
    
    for (i = 0; i < npids; ++i) {
        time_t end;
        ret = waitpid(pids[i], &end, 0);
        if (ret < 0) {
            warn("waitpid");
            return -1;
        }    
        else if (end < start) {
            say("hasher: failed. pid %d ended before all child processes "
                   "are created\n", pids[i]);
            pass = 0;
        }
	}
	
	if (pass)
	    say("hasher: test completed\n");
	    
    return 0;
}

