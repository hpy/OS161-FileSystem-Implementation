#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <endian.h>
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
#include <mips/trapframe.h>
#include <vm.h>


#define INVALID_FD(fd) (fd < 0 || fd >= OPEN_MAX)
#define INVALID_READ(flags) (!((flags & O_RDONLY) || (flags & O_RDWR) ))
#define INVALID_WRITE(flags) (!((flags & O_WRONLY) || (flags & O_RDWR) ))


static int validflag(int flag, int io_type);

static int validflag(int flag, int io_type){
    if(io_type==UIO_WRITE){
        return !(INVALID_WRITE(flag));
        // switch (flag) {
        //     case O_WRONLY:
        //     case O_RDWR: return 0;
        //     default: return EDOM;
        // }
    }
    if(io_type==UIO_READ){
        return (!(INVALID_READ(flag)) ||  (flag==O_RDONLY));
        // switch (flag) {
        //     case O_RDONLY:
        //     case O_RDWR: return 0;
        //     default: return EDOM;
        // }
    }
    return 0;
}


static int curproc_fdt_acquire(struct vnode *vn, int flags, mode_t mode, int *retval);
static int curproc_fdt_destroy(int fd);
static int sys_io(int fd, const void *buf, size_t nbytes, ssize_t *retval, int uio_rw_flag);

/*
    int sys_open(const char *filename, int flags, mode_t mode, int *retval)

    TODO check all the error codes are teh correct ones to return
*/

int sys_open(const char *filename, int flags, mode_t mode, int *retval){
    //kprintf("Opening: %s\n",filename); //temp
    //kprintf("SUCCESSFULLY START! Flag was: %d\n",flags); //temp
    if(filename==NULL){
        return EFAULT;
    }

    if(curproc_fdt==NULL){
        return EFAULT;
    }

    if (curproc_fdt->count >= OPEN_MAX){
        return EMFILE;
    }

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
    return 0;
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
    entry->ref_cnt = 1;
    entry->oft_mutex = lock_create("oft_mutex"); //maintains mutual exclusion between child/parent
    if(entry->oft_mutex == NULL){
        kfree(entry);
        return ENOMEM;
    }

    //lock the fdt
    lock_acquire(curproc_fdt->fdt_mutex);

    if (curproc_fdt->count >= OPEN_MAX){
        kfree(entry);
        lock_release(curproc_fdt->fdt_mutex);
        return EMFILE;
    }


    *retval = -1;

    //find first available fd entry
    for(int i = 0; i<OPEN_MAX; i++){
        if(curproc_fdt_entry(i)==NULL){
            curproc_fdt_entry(i) = entry;
            //set fd value
            *retval = i;
            break;
        }
    }
    //if we somehow didnt find an entry return error
    if(*retval == -1){
        kfree(entry);
        lock_release(curproc_fdt->fdt_mutex);
        return EMFILE;
    }

    curproc_fdt->count++;

    lock_release(curproc_fdt->fdt_mutex);

    return 0; //change these values to constants
}


//note caller must hold mutex for the fdt table for their process!
static int curproc_fdt_destroy(int fd){

    struct oft_entry *oft_entry = curproc_fdt_entry(fd);
    if (oft_entry==NULL){
        return EMFILE;
    }

    lock_acquire(curproc_fdt->fdt_mutex);

    if (curproc_fdt->count <= 0){
        lock_release(curproc_fdt->fdt_mutex);
        return EMFILE;
    }

    lock_acquire(oft_entry->oft_mutex);

    //only close fd if no other processes using it (dup2 and fork)
    if(oft_entry->ref_cnt > 1){
        oft_entry->ref_cnt--;
        curproc_fdt_entry(fd) = NULL;
        lock_release(oft_entry->oft_mutex);
    }else{
        vfs_close(oft_entry->vn);
        lock_release(oft_entry->oft_mutex);
        lock_destroy(oft_entry->oft_mutex);
        kfree(oft_entry);
        curproc_fdt_entry(fd) = NULL;
    }

    curproc_fdt->count--;
    lock_release(curproc_fdt->fdt_mutex);

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
    return curproc_fdt_destroy(fd);
}

/*
    int sys_io(int fd, void *buf, size_t nbytes, ssize_t *retval,  int rw_flag, int uio_rw_flag)
*/
static int sys_io(int fd, const void *buf, size_t nbytes, ssize_t *retval, int uio_rw_flag) {

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

    //should we hold the fdt_mutex to prevent deadlocks elsewhere?
    //lock_acquire(curproc_fdt->fdt_mutex);
    lock_acquire(oft->oft_mutex);

    struct iovec iov;
    struct uio uio;
    int result;

    if(!validflag(oft->flags, uio_rw_flag)) {
        lock_release(oft->oft_mutex);
        //lock_release(curproc_fdt->fdt_mutex);
        return EBADF;
    }

    uio_kinit(&iov, &uio, (void *)buf, nbytes, oft->seek_pos, uio_rw_flag);

    if (uio_rw_flag == UIO_WRITE) {
        result = VOP_WRITE(oft->vn, &uio);
        if (result) {
            lock_release(oft->oft_mutex);
            //lock_release(curproc_fdt->fdt_mutex);
            return result;
        }
    } else {
        result = VOP_READ(oft->vn, &uio);
        if (result) {
            lock_release(oft->oft_mutex);
            //lock_release(curproc_fdt->fdt_mutex);
            return result;
        }
    }

    //update the seek position
    oft->seek_pos = uio.uio_offset;

    //set number of bytes written
    *retval = nbytes - uio.uio_resid;

    lock_release(oft->oft_mutex);
    //lock_release(curproc_fdt->fdt_mutex);

    return 0;
}

/*
    int sys_write(int fd, void *buf, size_t nbytes, ssize_t *retval)
*/
int sys_write(int fd, const void *buf, size_t nbytes, ssize_t *retval){
    return sys_io(fd, buf, nbytes, retval, UIO_WRITE);
}

/*
    int sys_read(int fd, void *buf, size_t nbytes, ssize_t *retval)
*/
int sys_read(int fd, const void *buf, size_t nbytes, ssize_t *retval){
    return sys_io(fd, buf, nbytes, retval, UIO_READ);
}

/*
    int sys_dup2(int oldfd, int newfd, int *retval)
*/
int sys_dup2(int oldfd, int newfd, int *retval){
    if(curproc_fdt==NULL){
        return EBADF;
    }

    if (INVALID_FD(oldfd) || INVALID_FD(newfd)){
        return EBADF;
    }

    struct oft_entry *old_oft = curproc_fdt_entry(oldfd);
    struct oft_entry *new_oft = curproc_fdt_entry(newfd);

    lock_acquire(curproc_fdt->fdt_mutex);
    //check oldfd is valid (grab lock so noone can delete it  before i get lock!)
    if(old_oft==NULL){
        lock_release(curproc_fdt->fdt_mutex);
        return EBADF;
    }

    lock_acquire(old_oft->oft_mutex);

    //if newfd already exists, close newfd, and replace with oldfd
    if(new_oft!=NULL){
        lock_acquire(new_oft->oft_mutex);
        int chk = curproc_fdt_destroy(newfd);
        if(chk){
            lock_release(new_oft->oft_mutex);
            lock_release(old_oft->oft_mutex);
            lock_release(curproc_fdt->fdt_mutex);
            return chk;
        }
    }

    //point new_oft to the old_oft entry
    new_oft = old_oft;
    new_oft->ref_cnt++;
    *retval = newfd;

    lock_release(old_oft->oft_mutex);
    lock_release(curproc_fdt->fdt_mutex); //must prevent another syscall changing this oft_entry

    return 0;
}

/*
    int sys_lseek(int fd, off_t pos, int whence, off_t *retval)
*/
int sys_lseek(int fd, off_t pos, int whence, int *retval, struct trapframe *tf){

    off_t retval64;
    struct stat *fstat = {0};
    int file_size, result = 0;
    uint64_t offset;

    if (INVALID_FD(fd)) {
        return EBADF;
    }

    if(curproc_fdt==NULL){
        return EMFILE;
    }

    join32to64(tf->tf_a2, tf->tf_a3, &offset);

    result = copyin((userptr_t)tf->tf_sp + 16, &whence, sizeof(int));
    if (result) {
        return result;
    }

    // NOTE isseekable may return 1 for success
    struct oft_entry *oft_entry = curproc_fdt_entry(fd);
    if (!(VOP_ISSEEKABLE(oft_entry->vn))) {
        return ESPIPE;
    }

    if ((whence + pos) < 0) {
        return EINVAL;
    }

    switch (whence) {
        case SEEK_SET:
            oft_entry->seek_pos = offset;
            break;
        case SEEK_CUR:
            oft_entry->seek_pos += offset;
            break;
        case SEEK_END:
            result = VOP_STAT(oft_entry->vn, fstat);
            if (result) {
                return result;
            }
            file_size = fstat->st_size;
            oft_entry->seek_pos = file_size + offset;
            break;
        default:
            return EINVAL;
            break;
    }

    retval64 = oft_entry->seek_pos;

    split64to32(retval64, &tf->tf_v0, &tf->tf_v1);

    //need to set this to v0 as it overwrites tf_v0 in syscall
    *retval = (uint32_t)tf->tf_v0;

    return 0;
}
/*
    pid_t fork(void)
*/
int sys_fork(pid_t *retval){


    return 0;
}


/*
    getpid returns the process id of the current process.
	getpid does not fail.
*/
int sys_getpid(pid_t *retval){


    return 0;
}
