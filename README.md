os161
=====

The purpose of this repository to keep a copy of the latest starter code for 
os-161, presumably for the ece344 course offered by University of Toronto. We 
continue to make changes to the start code to make it easier for students to
design and test their implementation.

major updates
-------------

* added/modified various testbin programs used by os161-tester
* improved make process so depend.mk does not need to be checked into repository
* moved all executables into root directory under build folder so that so students cannot access DISK#.img 

assignment 1
------------

### Deliverables

* `df` command: toggle debug flags
* `dbflags` command:  view current debug flags
* `read` and `write` system call for console input and output
* `sleep` system call
* `__time` system call
* `_exit` system call


assignment 2
------------

### Deliverables

* lock implementation
* cv implementation
* catsem or catlock
* stoplight

assignment 3
------------

### Deliverables

* `waitpid` system call
* `fork` system call
* `getpid` system call
* `execv` system call
* `_exit` system call (needs to be updated)
* menu synchronization
* menu arguments (similar to execv)

### TODO

* `pipe` system call (to allow communication between processes)

assignment 4
------------

### Deliverables

* `sbrk` system call
* page reclamation
* demand paging
* swap
* copy-on-write

