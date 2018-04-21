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


//TEMP here
static int curfdt_acquire(struct vnode *vn, int flags, mode_t mode, int *retval);
// static int curfdt_destroy(int fd);

/*
    int sys_open(const char *filename, int flags, mode_t mode, int *retval)
*/
int sys_open(const char *filename, int flags, mode_t mode, int *retval){
    kprintf("Opening: %s\n",filename);
    if(filename==NULL){
        return EFAULT;
    }
    //lock_acquire(open_mutex);

    if(curproc->fdt==NULL){
        return EMFILE;
    }

    if (curfdt->count == __OPEN_MAX){
        //lock_release(open_mutex);
        return EMFILE;
    }

    //NOTE: does vfs_open check only valid modes entered?

    struct vnode *vn;
    int result = vfs_open ((char *)filename, flags, mode, &vn);
    if(result){
        return result;
    }
    //acquire the fd
    result = curfdt_acquire(vn, flags, mode, retval);
    //lock_release(open_mutex);
    kprintf("SUCCESSFULLY OPENED!\n");
    return result;
}



static int curfdt_acquire(struct vnode *vn, int flags, mode_t mode, int *retval){
    if (curfdt->count == __OPEN_MAX){
        return EMFILE;
    }

    //allocate file descriptor entry
    struct oft_entry *entry =  kmalloc(sizeof(struct oft_entry));
    if(entry==NULL){
        vfs_close(vn);
        return ENOMEM;
    }

    entry->vn = vn;
    entry->mode = mode;
    entry->flags = flags;
    entry->uio = 0;
    entry->seek_pos = 0;

    for(int i = 0; i<__OPEN_MAX; i++){
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
// static int curfdt_destroy(int fd){
//     if(curfdt==NULL){
//         return EMFILE;
//     }
//     if (curfdt->count <= 0){
//         return EMFILE;
//     }
//     if (curfdt->fdt_entry[fd]){
//         return EMFILE;
//     }
//     kfree(curfdt->fdt_entry[fd]);
//     curfdt->count--;
//     return 0;
// }


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
