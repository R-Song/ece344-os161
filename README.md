os161
=====

This repository contains the completed code for ece344 winter session. ASST1, ASST2, and ASST3 basically received full
marks while ASST4 passes all tests except for parallelvm. Quite frankly, we don't even come close to passing parallelvm. If we had more time, this would have been a fun challenge to work towards. It really requires an efficient VM system a good page eviction algorithm
and TLB replacement/management. The goal of this README is to talk about each assignment and perhaps give some tips and hints as well
as some of the troubles we had along the way. 


ASST1
------------

This assignment provides a taste of what is to come, specifically trying hard to understand a ton of given source code.
Debug flags are simple enough to implement. The system calls are a little more challenging. The key is to understand how
copyin and copyout work, and what their purpose is. Its important to remember that user memory and kernel memory are seperate.
When working with user pointers, such as in the case of read or write, you have to make sure the address that the user gives you
is a safe address. This is why we use copyin and copyout. We don't ever want the kernel to crash due to a bad user input.
Once you understand how to copyin and copyout, it is only a matter of using kprintf() and kgets() to write/read from the 
console.


ASST2  
------------

Although the code for locks is short, it is absolutely critical that they work perfectly. If not, you will have a miserable
time in assignment 4, when locks become neccessary to keep your VM system working properly. You might be wondering, locks are 
written by disabling/enabling interrupts using spl.h functions. Why can we just use interrupts all the time? Although for the most 
part its safe to just disable interrupts, sometimes it isnt. For example, say you turn off interrupts, do some things, then read from 
a file. Upon accessing the file, the kernel makes the current thread go to sleep until the disk I/O is done. When the thread sleeps, another thread can then enter the critical region. This is why sometimes you need locks.
As for the traffic problem, note that as long as you have no more than three cars in the intersection you won't have deadlock :)
Of course I wasn't so bright and wrote it with locks.


ASST3
------------

### Deliverables

This assignment is considerably harder than the previous. However, if you know the high level concepts, it really isnt that bad.

* `waitpid` system call
* `fork` system call
* `getpid` system call
* `execv` system call
* `_exit` system call (needs to be updated)
* menu synchronization
* menu arguments (similar to execv)

### TODO

* `pipe` system call (to allow communication between processes)

ASST4
------------

### Deliverables

* `sbrk` system call
* page reclamation
* demand paging
* swap
* copy-on-write

