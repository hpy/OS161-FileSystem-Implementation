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
#define INVALID_PERMS(flags, perm) (flags & perm)

static int curfdt_acquire(struct vnode *vn, int flags, mode_t mode, int *retval);
static int curfdt_destroy(int fd);

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
    kprintf("Opening: %s\n",filename); //temp

    if(filename==NULL){
        return EFAULT;
    }

    // make a kernel copy of the filepath if it is from userspace
    char *file = (char *)filename;
    int result;
    size_t got_len = 0;

    if ((vaddr_t)filename < USERSPACETOP) {
        result = copyinstr((const_userptr_t)filename, file, PATH_MAX, &got_len);
        if(result){
            kprintf("ERROR copying instruction failed: %d\n",result);
            kfree(file);
            return result;
        }
    }
    //lock_acquire(open_mutex);

    if(curproc->fdt==NULL){
        return EMFILE;
    }

    if (curfdt->count == OPEN_MAX){
        //lock_release(open_mutex);
        kprintf("TRYING! FD: %d\n",*retval); //temp
        return EMFILE;
    }

    //NOTE: does vfs_open check only valid modes entered? What if i send invalid modes?

    //retrieve our vnode
    struct vnode *vn;
    result = vfs_open ((char *)filename, flags, mode, &vn);
    if(result){
        return result;
    }

    //create our fd object and get our fd number
    result = curfdt_acquire(vn, flags, mode, retval);
    if(result){
        vfs_close(vn);
        return result;
    }
    //lock_release(open_mutex);
    kprintf("SUCCESSFULLY OPENED! FD: %d\n",*retval); //temp
    return result;
}



static int curfdt_acquire(struct vnode *vn, int flags, mode_t mode, int *retval){
    if(curfdt==NULL){
        return EMFILE;
    }

    //check fdt table is not full
    if (curfdt->count == OPEN_MAX){
        return EMFILE;
    }

    //allocate file descriptor entry
    struct oft_entry *entry =  kmalloc(sizeof(struct oft_entry));
    if(entry==NULL){
        return ENOMEM;
    }

    entry->vn = vn;
    entry->mode = mode;
    entry->flags = flags;
    entry->uio = 0;
    entry->seek_pos = 0;

    //find first available fd entry
    for(int i = 0; i<OPEN_MAX; i++){
        if(curfdt->fdt_entry[i]==NULL){
            curfdt->fdt_entry[i] = entry;
            //set fd value
            *retval = i;
            break;
        }
    }
    //do we need to check again here that oft_entry was assigned a location in the for loop?
    curfdt->count++;
    return 0; //change these values to constants
}

//not sure if this is enough...
static int curfdt_destroy(int fd){
    if(curfdt==NULL){
        return EMFILE;
    }
    if (curfdt->count <= 0){
        return EMFILE;
    }
    if(fd >= OPEN_MAX || fd < 0){
        return EMFILE;
    }
    if (curfdt->fdt_entry[fd]==NULL){
        return EMFILE;
    }
    kfree(curfdt->fdt_entry[fd]);
    curfdt->count--;
    return 0;
}


/*
    int sys_close(int fd)
*/
int sys_close(int fd){
    if(fd >= OPEN_MAX || fd < 0){
        return EMFILE;
    }
    kprintf("sys_close: WIP: Closing %d\n",fd); //temp

    return curfdt_destroy(fd); //what should sysclose return here?
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

    // EBADF (fd not valid or file doesnt have write perms)
    // EFAULT (part or all of bufs address space is invalid)
    // ENOSPC (there is no free space remaining on the filesystem)
    // EIO (hardware io error occured whilst trying to write)
    /*
    if (INVALID_FD(fd) || INVALID_PERMS(curfdt->fdt_entry[fd]->flags, O_WRONLY)) {
        return EBADF;
    }
    */

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
    if (INVALID_FD(fd) || INVALID_PERMS(curfdt->fdt_entry[fd]->flags, O_RDONLY)) {
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
