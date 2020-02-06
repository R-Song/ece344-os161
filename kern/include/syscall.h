#ifndef _SYSCALL_H_
#define _SYSCALL_H_

/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);

int sys_write(int fd, const void *buf, size_t nbytes, int *retval);

int sys_read(int fd, void *buf, size_t buflen, int *retval);

unsigned int sys_sleep(unsigned int seconds);

time_t sys___time(time_t *seconds, unsigned long *nanoseconds, int *retval);

#endif /* _SYSCALL_H_ */
