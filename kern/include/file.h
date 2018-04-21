/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
    file related data structures
*/

struct fdt {
	int count; 		    /* Number of fdt entries */
	struct oft_entry *fdt_entry[OPEN_MAX];
};

struct oft_entry {
	int uio;                   /* file in/out */
	struct vnode *vn;             /*if they share a common vnode then lock spinlock when writing*/
	mode_t mode;       /* if mode is read then only read actions, can access this fd */
    int flags;
    off_t seek_pos;     /* position in file (for lseek) */
};


/*
    On success, open returns a nonnegative file handle. On error, -1 is returned,
    and errno is set according to the error encountered.
*/
int sys_open(const char *filename, int flags, mode_t mode, int *retval);

/*
    On success, close returns 0. On error, -1 is returned, and errno is set
    according to the error encountered.
*/
int sys_close(int fd);

/*
    The count of bytes read is returned. This count should be positive.
    A return value of 0 should be construed as signifying end-of-file.
    On error, read returns -1 and sets errno to a suitable error code
    for the error condition encountered.
*/
int sys_write(int fd, const void *buf, size_t nbytes, ssize_t *retval);

/*
    The count of bytes written is returned. This count should be positive.
    A return value of 0 means that nothing could be written, but that no error occurred;
    this only occurs at end-of-file on fixed-size objects. On error, write returns -1
    and sets errno to a suitable error code for the error condition encountered.
*/
int sys_read(int fd, const void *buf, size_t nbytes, ssize_t *retval);

/*
    dup2 returns newfd. On error, -1 is returned, and errno is set
    according to the error encountered.
*/
int sys_dup2(int oldfd, int newfd, int *retval); //returns newfd on success

/*
    On success, lseek returns the new position. On error, -1 is returned,
    and errno is set according to the error encountered.
*/
int sys_lseek(int fd, off_t pos, int whence, int *retval);

/*
    On success, fork returns twice, once in the parent process and once in the child process.
    In the child process, 0 is returned. In the parent process,
    the process id of the new child process is returned.
    On error, no new process is created. fork, only returns once, returning -1,
    and errno is set according to the error encountered.
*/
//pid_t fork(void);

#endif /* _FILE_H_ */
