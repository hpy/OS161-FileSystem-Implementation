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
#include <proc.h>

#include <vm.h>


#define INVALID_FD(fd) (fd < 0 || fd >= OPEN_MAX)
#define INVALID_PERMS(flags, perm) ((flags) & perm)

static int curproc_fdt_acquire(struct vnode *vn, int flags, mode_t mode, int *retval);
static int curproc_fdt_destroy(int fd);

/*
    int sys_open(const char *filename, int flags, mode_t mode, int *retval)

    IMPORANT NOTE:
    open returns a file handle suitable for passing to read, write, close, etc.
    This file handle must be greater than or equal to zero.
    Note that file handles 0 (STDIN_FILENO), 1 (STDOUT_FILENO),
    and 2 (STDERR_FILENO) are used in special ways and are typically
    assumed by user-level code to always be open.

    TODO check all the error codes are teh correct ones to return
*/

int sys_open(const char *filename, int flags, mode_t mode, int *retval){
    //kprintf("Opening: %s\n",filename); //temp

    if(filename==NULL){
        return EFAULT;
    }

    if(curproc_fdt==NULL){
        return EFAULT;
    }

    if (curproc_fdt->count >= OPEN_MAX){
        return EMFILE;
    }
    //kprintf("%d\n",curproc_fdt->count);

    //copy the filename string safely from userspace to kernelspace
    int result;
    char *file = kmalloc(sizeof(char)*PATH_MAX);
    if(file == NULL){
        return EMFILE;
    }

    if ((vaddr_t)filename < USERSPACETOP){
        size_t got_len = 0;
        result = copyinstr((const_userptr_t)filename, file, PATH_MAX, &got_len);
        if(result){
            return result;
        }
    }else{
        if(strlen(filename)>PATH_MAX){
            return ENAMETOOLONG;
        }
        strcpy(file,filename);
    }

    //retrieve our vnode
    struct vnode *vn;
    result = vfs_open (file, flags, mode, &vn);
    if(result){
        kfree(file);
        return result;
    }
    kfree(file);

    //create our fd object and get our fd number
    result = curproc_fdt_acquire(vn, flags, mode, retval);
    if(result){
        vfs_close(vn);
        return result;
    }

    //kprintf("SUCCESSFULLY OPENED! FD: %d\n",*retval); //temp
    return result;
}



static int curproc_fdt_acquire(struct vnode *vn, int flags, mode_t mode, int *retval){
    //allocate file descriptor entry

    struct oft_entry *entry =  kmalloc(sizeof(struct oft_entry));
    if(entry==NULL){
        return ENOMEM;
    }

    entry->vn = vn;
    entry->mode = mode;
    entry->flags = flags;
    entry->seek_pos = 0;

    //find first available fd entry
    for(int i = 0; i<OPEN_MAX; i++){
        if(curproc_fdt_entry(i)==NULL){
            curproc_fdt_entry(i) = entry;
            //set fd value
            *retval = i;
            break;
        }
    }

    //do we need to check again here that oft_entry was assigned a location in the for loop?
    curproc_fdt->count++;
    return 0; //change these values to constants
}

//not sure if this is enough...
static int curproc_fdt_destroy(int fd){
    if (curproc_fdt_entry(fd)==NULL){
        return EMFILE;
    }
    kfree(curproc_fdt_entry(fd));
    curproc_fdt_entry(fd) = NULL;
    curproc_fdt->count--;
    return 0;
}


/*
    int sys_close(int fd)
*/
int sys_close(int fd){
    if(INVALID_FD(fd)){
        return EMFILE;
    }
    if(curproc_fdt==NULL){
        return EMFILE;
    }
    if (curproc_fdt->count <= 0){
        return EMFILE;
    }
    //kprintf("sys_close: WIP: Closing %d\n",fd); //temp
    return curproc_fdt_destroy(fd); //what should sysclose return here?
}

/*
    int sys_write(int fd, void *buf, size_t nbytes, ssize_t *retval)
*/
int sys_write(int fd, const void *buf, size_t nbytes, ssize_t *retval){

    //do they have perm on openfile to write
    if (INVALID_FD(fd)){
        return EBADF;
    }
    if(curproc_fdt==NULL){
        return EBADF;
    }

    struct oft_entry *oft = curproc_fdt_entry(fd);
    if(oft==NULL){
        return EBADF;
    }

    if(!INVALID_PERMS(oft->flags, O_WRONLY)) {
        return EBADF;
    }

    struct iovec iov;
    struct uio uio;
    int result;

    uio_kinit(&iov, &uio, (void *)buf, nbytes, oft->seek_pos, UIO_WRITE);

    result = VOP_WRITE(oft->vn, &uio);
    if (result) {
        return result;
    }

    //update the seek position
    oft->seek_pos = uio.uio_offset;

    //set number of bytes written
	*retval = nbytes - uio.uio_resid;

    return result;
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

    // EBADF (fd not valid or file doesnt have write perms)
    // EFAULT (part or all of bufs address space is invalid)
    // EIO (hardware io error occured whilst trying to write)
    /*
    if (INVALID_FD(fd) || INVALID_PERMS(curproc_fdt->fdt_entry[fd]->flags, O_RDONLY)) {
        return EBADF;
    }
    */
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
