/*
 * wait.c
 *
 * tests ordering of waitpid() and _exit() without needing arguments
 *
 * Test case 1:
 *      result expected: wekp
 * Test case 2:
 *      result expected: ewp
 * Test case 3:
 *      result expected: wer
 * Test case 4:
 *      result expected: arcsp 
 * Test case 5: 
 *      result expected: att
 * Test case 6:
 *      result expected: acp
 *
 * Authors:
 * Kuei Sun <kuei.sun@mail.utoronto.ca>
 * Ashvin Goel <ashvin@eecg.toronto.edu>
 *
 * University of Toronto, 2016
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

static volatile int mypid;
int pid_p, pid_c;

static int dofork(void)
{
        int pid;
        pid = fork();
        if (pid < 0) {
                warn("fork");
        }
        return pid;
}

/*
 * copied from address space test in forktest
 */
static void check(void)
{
        int i;

        mypid = getpid();
        
        /* Make sure each fork has its own address space. */
        for (i=0; i<800; i++) {
                volatile int seenpid;
                seenpid = mypid;
                if (seenpid != getpid()) {
                        errx(1, "pid mismatch (%d, should be %d) "
                             "- your vm is broken!", 
                             seenpid, getpid());
                }
        }
}

#define dowait(pid) __dowait((pid), 0)
#define dowait2(pid, ch) __dowait((pid), (ch))

/*
 * based on dowait in forktest
 */
static void __dowait(int pid, char ch)
{
        int x;

        if (pid<0) {
                /* fork in question failed; just return */
                return;
        }
        if (pid==0) {
                /* we were the child in the fork -- exit */
                exit(0);
        }
        if (waitpid(pid, &x, 0)<0) {
                /* EINVAL is the only accepted errno */
                if (ch == 0 || errno != EINVAL)
                        warn("waitpid");
                else
                        putchar(ch);
        }
        else if (x!=0)
                warnx("pid %d: exit %d", pid, x);                
}

#define TEST_BEGIN(num, ...) void wait ## num(void) { __VA_ARGS__; printf(#num " ");
#define TEST_END() }

TEST_BEGIN(1)
        pid_p = getpid();
        putchar('w');
        pid_c = dofork();

        if (getpid() == pid_p) {
                check();
                dowait(pid_c);
        } else {
                putchar('e');
                exit(0);
        }

        putchar('k');
        if (getpid() == pid_p)
                putchar('p');
        else 
                printf("wrong %d\n", getpid());

        putchar('\n');
TEST_END()

TEST_BEGIN(2)
        pid_p = getpid();
        putchar('e');
        pid_c = dofork();

        if (getpid() == pid_p)
                check();

        if (getpid() != pid_p) {
                check();
                exit(0);
        } else {
                putchar('w');
                dowait(pid_c);
        }

        putchar('p');
        putchar('\n');
TEST_END()

TEST_BEGIN(3)
        pid_p = getpid();
        putchar('w');
        pid_c = dofork();

        if (getpid() == pid_p)
                dowait(pid_c);

        if (getpid() != pid_p) {
                check();
                putchar('e');
                exit(0);
        } 

        if (getpid() == pid_p)
                dowait2(pid_c, 'r');
        else
                printf("wrong!\n");

        putchar('\n');
TEST_END()

TEST_BEGIN(4, int pid_s)
        pid_p = getpid();          
        putchar('a');
        pid_s = dofork();          

        if (getpid() == pid_p)
                check(); 

        if (getpid() == pid_p) {
                pid_c = dofork();
        } else {
                /* sibling here, just sleep */
                sleep(1);
                putchar('s');
                exit(0);
        }              
        
        if (getpid() == pid_p)  
                check();
        
        if (getpid() == pid_p) {
                dowait(pid_c); 
                dowait(pid_s);
        } else {
                /* try to wait for its sibling */
                dowait2(pid_s, 'r');
                putchar('c');
                exit(0);
        }
        
        putchar('p');
        putchar('\n');
TEST_END()

TEST_BEGIN(5, int pid)
        pid = dofork();
        if (pid > 0) {
                dowait(pid);
                return;
        }

        pid_p = getpid();
        putchar('a');
        pid_c = dofork();

        if (getpid() == pid_p) {
                check();
                putchar('t');
                exit(0);
        } else {
                putchar('t');
                exit(0);
        }

        putchar('e');
        putchar('\n');
TEST_END()

TEST_BEGIN(6)
        pid_p = getpid();          
        putchar('a');
        pid_c = dofork();          

        if (getpid() == pid_p)  
                check();               

        if (getpid() == pid_p) {
                dowait(pid_c);
                putchar('p');
                putchar('\n');
                exit(0);            
        } else {
                putchar('c');          
                exit(0);              
        }

        putchar('e');              
        putchar('\n');             
TEST_END()

int main(void)
{
        wait1();
        wait2();
        wait3();
        wait4();
        wait5();
        putchar('\n');
        wait6();
        return 0;
}
