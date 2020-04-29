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
a file. Upon accessing the file, the kernel makes the current thread go to sleep until the disk I/O is done. When the thread sleeps, 
another thread can then enter the critical region. This is why sometimes you need locks.
As for the traffic problem, note that as long as you have no more than three cars in the intersection you won't have deadlock :)
Of course I wasn't so bright and wrote it with locks.


ASST3
------------

This assignment is considerably harder than the previous. However, if you know the high level concepts, it really isnt that bad.
For fork, you have to make a copy of the user execution context. This involves copying the addrspace. After this, you utilize the
threadfork function to fork a thread that starts at md_fork_entry(). When that thread eventually gets scheduled, it will start
executing from there and immediately enter the user space. For wait and exit, you have to design a system that allows for communication
between a process that is waiting and another that is exitting. We recommend using a semaphore for this purpose. To keep track of processes
we used a static array. A dynamic one could work as well. Execv is trickier but a foundation is layed down by runprogram(). The only thing
that needs to be implemented is how to handle command line arguments. It involves loading contents to the user stack and giving the program
pointers to the arguments. A nice diagram can be found around sys_execv() in this repo.


ASST4
------------

The hardest of them all. This assignment is very long and is quite difficult. First start by creating a coremap. A coremap map keeps track
of the state of each of the physical pages. It should keep a state along with any other useful data. For our implementation we had it store
a pointer to the relevant page table entry. Once the coremap is implemented, alloc_kpages and free_kpages should work. This means you 
should not run out of memory by running the thread, sem, and cv tests. Implement Addrpspace functions next, focusing on getting as_copy to work
so that fork works. Next write vm_fault so you can actually run programs. When load_elf is called, it uses uiomove to load the program into memory.
This will trigger vm_faults, so make sure u handle TLB here. All TLB dealings should be atomic! Once this is all working, implement load on demand
to pass tests such as huge. This involves changing load_elf, and instead loading the pages in vm_fault. Consider using a mkuio rather than a user
uio. It might cause synchronize issues down the road. After this, do swapping. Learn how VOP_READ and VOP_WRITE work and design a system that keeps
track of the swap space. We used a bitmap for this purpose. NOTE: up until this point, one could get away by just disabling interrupts all the time.
IT WONT WORK NOW. Because reading off a disk is slow, the thread goes to sleep and waits to an interrupt from the disk once the read or write is done...
So make sure you use locks to synchronize and not just interrupts.

If one manages to get everything working with no synchronization issues and has the time to implement smarter TLB handling methods (not flushing every
single time). They should be able to get full marks.

