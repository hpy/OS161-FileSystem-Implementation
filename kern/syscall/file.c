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
static int sys_io(int fd, const void *buf, size_t nbytes, ssize_t *retval, int uio_rw_flag);


/*
    static int validflag(int flag, int io_type)

    Check if given flags are valid with io_type
    Example: O_RDONLY and O_WRONLY are not compatible
                    O_RDONLY and O_RDWR are compatible
*/
static int validflag(int flag, int io_type){
    if(io_type==UIO_WRITE){
        return !(INVALID_WRITE(flag));
    }
    if(io_type==UIO_READ){
        return (!(INVALID_READ(flag)) ||  (flag==O_RDONLY));
    }
    return EINVAL;
}


/*
    int sys_open(const char *filename, int flags, mode_t mode, int *retval)

    Opens a given filename, and returns a file handle suitable for passing to read, write, close, etc.
*/
int sys_open(const char *filename, int flags, mode_t mode, int *retval){
    if(curproc_fdt==NULL){
        return EINVAL;
    }
    if(filename==NULL){
        return EFAULT;
    }
    if(retval==NULL){
        return EINVAL;
    }

    /* intial check does not require fdt lock simply saves time from fail later on*/
    if (curproc_fdt->count >= OPEN_MAX){
        return EMFILE;
    }

    /* safely copy filename from userspace to kernelspace */
    int result;
    char *file = kmalloc(sizeof(char)*PATH_MAX);
    if(file == NULL){
        return EMFILE;
    }

    /* filename is in kernelspace*/
    if ((vaddr_t)filename >= USERSPACETOP){
        return EFAULT;
    }

    size_t got_len = 0;
    result = copyinstr((const_userptr_t)filename, file, PATH_MAX, &got_len);
    if(result){
        return result;
    }


    /* retrieve vnode */
    struct vnode *vn;
    result = vfs_open (file, flags, mode, &vn);
    if(result){
        kfree(file);
        return result;
    }
    kfree(file);

    /* create oft entry and allocate to process fdt */
    result = oft_acquire(vn, flags, mode, retval);
    if(result){
        vfs_close(vn);
        return result;
    }
    return 0;
}



/*
    static int oft_acquire(struct vnode *vn, int flags, mode_t mode, int *retval)

    Allocate a new oft entry and insert into first available slot in the process fdt table
*/
int oft_acquire(struct vnode *vn, int flags, mode_t mode, int *retval){
    if(curproc_fdt==NULL){
        return EINVAL;
    }
    if(vn==NULL){
        return EINVAL;
    }
    if(retval==NULL){
        return EINVAL;
    }

    /* initialise oft entry */
    struct oft_entry *oft_entry =  kmalloc(sizeof(struct oft_entry));
    if(oft_entry==NULL){
        return ENOMEM;
    }

    oft_entry->vn = vn;
    oft_entry->mode = mode;
    oft_entry->flags = flags;
    oft_entry->seek_pos = 0;
    oft_entry->ref_cnt = 1;

    oft_entry->oft_mutex = lock_create("oft_mutex");
    if(oft_entry->oft_mutex == NULL){
        kfree(oft_entry);
        return ENOMEM;
    }

    lock_acquire(curproc_fdt->fdt_mutex);
    if (curproc_fdt->count >= OPEN_MAX){
        kfree(oft_entry);
        lock_release(curproc_fdt->fdt_mutex);
        return EMFILE;
    }

    /* allocate oft entry into process fdt */
    for(int i = 0; i<OPEN_MAX; i++){
        if(curproc_fdt_entry(i)==NULL){
            curproc_fdt_entry(i) = oft_entry;
            *retval = i;
            break;
        }
    }

    curproc_fdt->count++;
    lock_release(curproc_fdt->fdt_mutex);

    return 0;
}



/*
    int sys_close(int fd)

    Closes requested file descriptor. Other file handles are not affected in any way,
    even if they are sharing the same file vnode or oft_entry (ie dup2 and fork).
*/
int sys_close(int fd){
    if(curproc_fdt==NULL){
        return EINVAL;
    }
    if(INVALID_FD(fd)){
        return EBADF;
    }

    lock_acquire(curproc_fdt->fdt_mutex);

    struct oft_entry *oft_entry = curproc_fdt_entry(fd);
    if (oft_entry==NULL){
        lock_release(curproc_fdt->fdt_mutex);
        return EFAULT;
    }

    lock_acquire(oft_entry->oft_mutex);

    if (curproc_fdt->count <= 0){
        lock_acquire(oft_entry->oft_mutex);
        lock_release(curproc_fdt->fdt_mutex);
        return EMFILE;
    }

    /* release and cleanup oft_entry and fdt pointer */
    /* do not cleanup oft_entry if other processes using it (dup2 and fork) */
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
    int sys_io(int fd, void *buf, size_t nbytes, ssize_t *retval,  int rw_flag, int uio_rw_flag)

    Read or write up to nbytes of a file specified by fd, at the seek position of the file.
    The file must have been opened with a valid matching read/write operation.
    Each read/write operation is atomic relative to other I/O to the same file.
*/
static int sys_io(int fd, const void *buf, size_t nbytes, ssize_t *retval, int uio_rw_flag) {
    if(curproc_fdt==NULL){
        return EINVAL;
    }
    if(buf==NULL){
        return EINVAL;
    }
    if(retval==NULL){
        return EINVAL;
    }
    if(INVALID_FD(fd)){
        return EBADF;
    }

    lock_acquire(curproc_fdt->fdt_mutex);
    struct oft_entry *oft_entry = curproc_fdt_entry(fd);
    if(oft_entry==NULL){
        lock_release(curproc_fdt->fdt_mutex);
        return EFAULT;
    }

    lock_acquire(oft_entry->oft_mutex);
    lock_release(curproc_fdt->fdt_mutex);

    struct iovec iov;
    struct uio uio;
    int result;

    /* check file read/write status matches request */
    if(!validflag(oft_entry->flags, uio_rw_flag)) {
        lock_release(oft_entry->oft_mutex);
        return EBADF;
    }

    /* initialise the uio structure */
    uio_kinit(&iov, &uio, (void *)buf, nbytes, oft_entry->seek_pos, uio_rw_flag);

    if (uio_rw_flag == UIO_WRITE) {
        result = VOP_WRITE(oft_entry->vn, &uio);
        if (result) {
            lock_release(oft_entry->oft_mutex);
            return result;
        }
    } else {
        result = VOP_READ(oft_entry->vn, &uio);
        if (result) {
            lock_release(oft_entry->oft_mutex);
            return result;
        }
    }

    /* update the seek position */
    oft_entry->seek_pos = uio.uio_offset;

    /* set number of bytes written */
    *retval = nbytes - uio.uio_resid;

    lock_release(oft_entry->oft_mutex);

    return 0;
}



/*
    int sys_write(int fd, void *buf, size_t nbytes, ssize_t *retval)

    Write up to nbytes to the file specified by fd, at the location in the file
    specified by the current seek position of the file, taking the data from
    the space pointed to by buf. The file must be open for writing.
    The current seek position of the file is advanced by the number of bytes written.
    Each write (or read) operation is atomic relative to other I/O to the same file.
*/
int sys_write(int fd, const void *buf, size_t nbytes, ssize_t *retval){
    return sys_io(fd, buf, nbytes, retval, UIO_WRITE);
}



/*
    int sys_read(int fd, void *buf, size_t nbytes, ssize_t *retval)

    Reads up to nbytes from the file specified by fd, at the location in the file
    specified by the current seek position of the file, and stores them in the space pointed to by buf.
    The file must be open for reading.
    The current seek position of the file is advanced by the number of bytes read.
    Each read (or write) operation is atomic relative to other I/O to the same file.
*/
int sys_read(int fd, const void *buf, size_t nbytes, ssize_t *retval){
    return sys_io(fd, buf, nbytes, retval, UIO_READ);
}



/*
    int sys_dup2(int oldfd, int newfd, int *retval)

    Clone the given old file descriptor onto the request newfd .
    If newfd names an already-open file, that file is closed.
    The two handles refer to the same "open" of the file -- that is,
    they are references to the oft_entry object and share the same seek pointer.
    Note that this is different from opening the same file twice.
*/
int sys_dup2(int oldfd, int newfd, int *retval){

    if(curproc_fdt==NULL){
        return EINVAL;
    }

    if (INVALID_FD(oldfd) || INVALID_FD(newfd)){
        return EBADF;
    }

    if (oldfd == newfd){
        *retval = oldfd;
        return 0;
    }

    struct oft_entry *old_oft = curproc_fdt_entry(oldfd);
    struct oft_entry *new_oft = curproc_fdt_entry(newfd);

    lock_acquire(curproc_fdt->fdt_mutex);
    if(old_oft==NULL){
        lock_release(curproc_fdt->fdt_mutex);
        return EBADF;
    }

    lock_acquire(old_oft->oft_mutex);

    /* if newfd already exists, close newfd, and replace with oldfd */
    if(new_oft!=NULL){
        lock_acquire(new_oft->oft_mutex);
        int chk = sys_close(newfd);
        if(chk){
            lock_release(new_oft->oft_mutex);
            lock_release(old_oft->oft_mutex);
            lock_release(curproc_fdt->fdt_mutex);
            return chk;
        }
    }

    lock_release(curproc_fdt->fdt_mutex);

    /* point new_oft to the old_oft entry */
    new_oft = old_oft;
    new_oft->ref_cnt++;
    *retval = newfd;

    lock_release(old_oft->oft_mutex);

    return 0;
}

/*
    int sys_lseek(int fd, int whence, off_t *retval)

    Alters the current seek position of the file handle fd and seeks to a new position
    based on pos and whence.
*/
int sys_lseek(int fd, int *retval, struct trapframe *tf){

    struct stat fstat;
    int file_size, whence, result = 0;
    int64_t offset;

    if(curproc_fdt==NULL){
        return EINVAL;
    }
    if (INVALID_FD(fd)) {
        return EBADF;
    }

    /*merge two arguments into one 64bit value */
    join32to64(tf->tf_a2, tf->tf_a3, (uint64_t*)&offset);

    /* copy from userspace into kernelspace */
    result = copyin((userptr_t)tf->tf_sp + 16, &whence, sizeof(int));
    if (result) {
        return result;
    }

    lock_acquire(curproc_fdt->fdt_mutex);

    struct oft_entry *oft_entry = curproc_fdt_entry(fd);
    if(oft_entry == NULL){
        lock_release(curproc_fdt->fdt_mutex);
        return EBADF;
    }

    lock_acquire(oft_entry->oft_mutex);
    lock_release(curproc_fdt->fdt_mutex);

    if (!(VOP_ISSEEKABLE(oft_entry->vn))) {
        lock_release(oft_entry->oft_mutex);
        return ESPIPE;
    }
//lseek(fd, -50, SEEK_CUR);
    switch (whence) {
        /* Seek relative to beginning of file */
        case SEEK_SET:
            if (offset < 0) {
                lock_release(oft_entry->oft_mutex);
                return EINVAL;
            }
            oft_entry->seek_pos = offset;
            break;
        /* Seek relative to current position in file */
        case SEEK_CUR:
            if ((oft_entry->seek_pos + offset) < 0) {
                lock_release(oft_entry->oft_mutex);
                return EINVAL;
            }
            oft_entry->seek_pos += offset;
            break;
        /* Seek relative to end of file */
        case SEEK_END:
            result = VOP_STAT(oft_entry->vn, &fstat);
            if (result) {
                lock_release(oft_entry->oft_mutex);
                return result;
            }
            file_size = fstat.st_size;
            if ((file_size + offset) < 0) {

                lock_release(oft_entry->oft_mutex);
                return EINVAL;
            }
            oft_entry->seek_pos = file_size + offset;
            break;
        default:
            lock_release(oft_entry->oft_mutex);
            return EINVAL;
            break;
    }

    /* Split 64bit value into seperate return arguments */
    split64to32(oft_entry->seek_pos, &tf->tf_v0, &tf->tf_v1);
    lock_release(oft_entry->oft_mutex);

    /* Set retval to v0 value as v0 is overwritten by syscall on return */
    *retval = (uint32_t)tf->tf_v0;

    return 0;
}
