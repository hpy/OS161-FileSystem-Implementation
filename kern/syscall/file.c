#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>




/*
    int sys_open(const char *filename, int flags, mode_t mode, int *retval)
*/
int sys_open(const char *filename, int flags, mode_t mode, int *retval){
    (void)filename;
    (void)flags;
    (void)mode;
    (void)retval;
    kprintf("sys_open: Not Implemented at this time\n");
    return -1;
}

/*
    int sys_close(int fd)
*/
int sys_close(int fd){
    (void)fd;
    kprintf("sys_close: Not Implemented at this time\n");
    return -1;
}

/*
    int sys_write(int fd, void *buf, size_t nbytes, ssize_t *retval)
*/
int sys_write(int fd, const void *buf, size_t nbytes, ssize_t *retval){
    (void)fd;
    (void)buf;
    (void)nbytes;
    (void)retval;
    kprintf("sys_write: Not Implemented at this time\n");
    return -1;
}

/*
    int sys_read(int fd, void *buf, size_t nbytes, ssize_t *retval)
*/
int sys_read(int fd, const void *buf, size_t nbytes, ssize_t *retval){
    (void)fd;
    (void)buf;
    (void)nbytes;
    (void)retval;
    kprintf("sys_read: Not Implemented at this time\n");
    return -1;
}

/*
    int sys_dup2(int oldfd, int newfd, int *retval)
*/
int sys_dup2(int oldfd, int newfd, int *retval){
    (void)oldfd;
    (void)newfd;
    (void)retval;
    kprintf("sys_dup2: Not Implemented at this time\n");
    return -1;
}

/*
    int sys_lseek(int fd, off_t pos, int whence, off_t *retval)
*/
int sys_lseek(int fd, off_t pos, int whence, int *retval){
    (void)fd;
    (void)pos;
    (void)whence;
    (void)retval;
    kprintf("sys_lseek: Not Implemented at this time\n");
    return -1;
}

/*
    pid_t fork(void)
*/
// pid_t fork(void){
//     kprintf("Not Implemented at this time\n");
//     return -1;
// }
