#ifndef _SYSCALL_H_
#define _SYSCALL_H_

/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);

int sys_write(int filehandle, const void *buf, size_t size);

int sys_read(int filehandle, void *buf, size_t size);

unsigned int sys_sleep(unsigned int seconds);

//int open(const char *filename, int flags, ...);
//int read(int filehandle, void *buf, size_t size);
//int write(int filehandle, const void *buf, size_t size);
//int close(int filehandle);
//int reboot(int code);
//int sync(void);
//unsigned int sleep(unsigned int seconds);

#endif /* _SYSCALL_H_ */
