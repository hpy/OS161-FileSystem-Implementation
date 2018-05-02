/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>
#include <mips/trapframe.h>

/*
    file related data structures
*/

struct fdt {
	int count; 		    /* Number of fdt entries */
	struct lock *fdt_mutex; 		/* maintain exclusion over fdt when modifying */
	struct oft_entry *fdt_entry[OPEN_MAX];
};

/*
    The optional mode argument provides the file permissions to use and
    is only meaningful in Unix, or if you choose to implement Unix-style
    security later on. it can be ignored in OS/161.
*/

struct oft_entry {
	struct lock *oft_mutex; 	/* lock to maintain mutual exclusion between dup and children */
	struct vnode *vn;  /*lock when writing or reading*/
	mode_t mode;       /* not used by os161 */
    int flags;               /* if mode is read then only read actions, can access this fd */
    off_t seek_pos;     /* position in file */
	int ref_cnt; 			/* count of handles to this entry used by dup2 and fork */
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
int sys_write(int fd, void *buf, size_t nbytes, ssize_t *retval);

/*
    The count of bytes written is returned. This count should be positive.
    A return value of 0 means that nothing could be written, but that no error occurred;
    this only occurs at end-of-file on fixed-size objects. On error, write returns -1
    and sets errno to a suitable error code for the error condition encountered.
*/
int sys_read(int fd, void *buf, size_t nbytes, ssize_t *retval);

/*
    dup2 returns newfd. On error, -1 is returned, and errno is set
    according to the error encountered.
*/
int sys_dup2(int oldfd, int newfd, int *retval); //returns newfd on success

/*
    On success, lseek returns the new position. On error, -1 is returned,
    and errno is set according to the error encountered.
*/
int sys_lseek(int fd, int *retval, struct trapframe *tf);



/*
    Allocates a new oft entry and attaches vnode and other fields.
*/
int oft_acquire(struct vnode *vn, int flags, mode_t mode, int *retval);


/*
    On success, fork returns the new child process id to the parent and 0 to the child. On error, -1 is returned,
    and errno is set according to the error encountered.
*/
int sys_fork(pid_t *retval);


/*
    getpid returns the process id of the current process.
	getpid does not fail.
*/
int sys_getpid(pid_t *retval);




#endif /* _FILE_H_ */
