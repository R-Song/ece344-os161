#ifndef _SYSCALL_H_
#define _SYSCALL_H_

struct trapframe;

/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

/* General systems calls */
int sys_reboot(int code);

int sys_write(int fd, const void *buf, size_t nbytes, int *retval);

int sys_read(int fd, void *buf, size_t buflen, int *retval);

int sys_sleep(unsigned int seconds);

int sys___time(time_t *seconds, unsigned long *nanoseconds, time_t *retval);


/* System calls related to processes */
int sys_fork(struct trapframe *tf, pid_t *retval);

int sys_getpid(pid_t *retval);

int sys_waitpid(pid_t pid, int *status, int options, pid_t *retval);

int sys__exit(int exitcode);

int sys_execv(const char *program, char **args, pid_t *retval);

int sys_sbrk(intptr_t amount, pid_t *retval);

#endif /* _SYSCALL_H_ */
