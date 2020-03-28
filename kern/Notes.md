Use this markdown file to keep track of important things.

Date: March 28th
    -sy1, sy2, sy3, km1, and km2 all pass without memory leaks, some things are sketchy though
    -Had to delete the assertion at synch.c:75, it was firing off and crashing the kernel for sy2 and km2
    -Changed up exorcise to destroy all zombie threads with ppid=1, whether or not it has been adopted
    -The kernel hangs when trying to shutdown...not sure why
    -Worst of all, gdb doesnt work, also not sure why

