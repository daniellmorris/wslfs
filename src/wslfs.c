/**
 * Windows subsystem for linux filesystem for the realy linux
 * gcc -Wall wslfs.c `pkg-config fuse --cflags --libs` -o wslfs
 *
 */


#define FUSE_USE_VERSION 30

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif
//#define HAVE_SETXATTR
#define HAVE_UTIMENSAT

#include <stdlib.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/types.h>

#include <sys/xattr.h>
//#ifdef HAVE_SETXATTR
//    #include <sys/xattr.h>
//#endif
#ifdef HAVE_UTIMENSAT
    #include <fcntl.h> /* Definition of AT_* constants */
#endif
#include <fuse.h>

#define MAX_PATHLEN 500

//// Note: Most of this is already defined in the Windows SDK... 
//// TODO: Use header files for these (Not sure what I'm missing yet)
////#define	S_IFMT	 0170000		/* type of file */
////#define	S_IFIFO	 0010000		/* named pipe (fifo) */
////#define	S_IFCHR	 0020000		/* character special */
////#define	S_IFDIR	 0040000		/* directory */
////#define	S_IFBLK	 0060000		/* block special */
//#define	S_IFREG	 0100000		/* regular */
//#define	S_IFLNK	 0120000		/* symbolic link */
////#define	S_IFSOCK 0140000		/* socket */
////#define	S_ISVTX	 0001000		/* save swapped text even after use */
////#define S_BLKSIZE	512		/* block size used in the stat struct */
//
//// TODO: Fix this - Not sure why this isn't in the header allready
//#define AT_SYMLINK_NOFOLLOW     0x100   /* Do not follow symbolic links.  */


#define WSL_DEFAULT_FLAGS 0x0
#define WSL_VERSION  0x0001
typedef struct __wsl_extended_attr_v1__
{
    uint32_t pad1;    
    uint32_t pad2;    
    uint32_t pad3;   
    uint32_t pad4;   
    uint16_t Flags;
    uint16_t Version;
    uint32_t st_mode;       // Mode bit mask constants: https://msdn.microsoft.com/en-us/library/3kyc8381.aspx
    uint32_t st_uid;        // Numeric identifier of user who owns file (Linux-specific).
    uint32_t st_gid;        // Numeric identifier of group that owns the file (Linux-specific)
    uint32_t st_rdev;       // Drive number of the disk containing the file.
    uint32_t st_atime_nsec; // Time of last access of file (nano-seconds).
    uint32_t st_mtime_nsec; // Time of last modification of file (nano-seconds).
    uint32_t st_ctime_nsec; // Time of creation of file (nano-seconds).
    uint64_t st_at;    // Time of last access of file.
    uint64_t st_mt;    // Time of last modification of file.
    uint64_t st_ct;    // Time of creation of file.
} wsl_extended_attr_v1_t;

struct wsl_dirp
{
    DIR *dp;
    struct dirent *entry;
    off_t offset;
};

// TODO:
/* root of the filesystem */
char *root;
uint32_t rootLen = 0;

static inline void sanitize_path(const char* path, char *outpath)
{
    if (!path) {
        outpath = root;
    } else if (path[0]=='/'){
        snprintf(outpath, MAX_PATHLEN, "%s%s", root, path);
    } else {
        snprintf(outpath, MAX_PATHLEN, "%s/%s", root, path);
    }
    //fprintf(stdout, "DEBUG sanitize_path: (%s)\n", outpath);
}
static inline void sanitize_path_with_dir(const char* path, const char * dir, char *outpath)
{
    if (!path) {
        outpath = root;
    } else if (path[0]=='/'){
        snprintf(outpath, MAX_PATHLEN, "%s%s%s", root, dir, path);
    } else {
        snprintf(outpath, MAX_PATHLEN, "%s%s/%s", root, dir, path);
    }
//    sanitize_path(dir, outpath)
//    sanitize_path(outpath, path);
}


//static void debug_output_wsl_extend_attr(char * path, wsl_extended_attr_v1_t * ext) {
//    fprintf(stdout, "debug_output_wsl_extend_attr: (%s) %04x\n", path, ext->st_mode );
//    fprintf(stdout,"pad1:                     %08x\n", ext->pad1);
//    fprintf(stdout,"pad2:                     %08x\n", ext->pad2);
//    fprintf(stdout,"pad3:                     %08x\n", ext->pad3);
//    fprintf(stdout,"pad4:                     %08x\n", ext->pad4);
//    fprintf(stdout,"Flags:                     %04x\n", ext->Flags);
//    fprintf(stdout,"Version:                   %04x\n", ext->Version);
//    fprintf(stdout,"Mode:                      %04o (octal)\n", ext->st_mode  );
//    fprintf(stdout,"RDev:                      %04x\n", ext->st_rdev  );
//    fprintf(stdout,"uid:                      %d\n", ext->st_uid  );
//    fprintf(stdout,"gid:                      %d\n", ext->st_gid  );
//}

static int get_extended_attr(const char *path, wsl_extended_attr_v1_t * ext) {
    char * name = "system.ntfs_ea";

    memset(ext, 0, sizeof(wsl_extended_attr_v1_t));
    int res = lgetxattr(path, name, ext, sizeof(wsl_extended_attr_v1_t));
    return res;
}

static int set_extended_attr(const char *path, wsl_extended_attr_v1_t * ext) {
    char * name = "system.ntfs_ea";

    int res = lsetxattr(path, name, ext, sizeof(wsl_extended_attr_v1_t), 0);
    
    return res;
}

static int wsl_getattr(const char *path, struct stat *stbuf)
{
    int res;
    char p[MAX_PATHLEN];
    wsl_extended_attr_v1_t ext;
   
    sanitize_path(path, p);

    res = lstat(p, stbuf);
    if (res == -1)
        return -errno;

    int res2 = get_extended_attr(p, &ext);
    if (res2 == -1)
        return -errno;
   

    stbuf->st_mode = ext.st_mode;//(stbuf->st_mode & ~(S_IRWXU | S_IRWXG | S_IRWXO)) & (ext.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
    stbuf->st_atime = ext.st_at;
    stbuf->st_mtime = ext.st_mt;
    stbuf->st_ctime = ext.st_ct;
    stbuf->st_uid = ext.st_uid;
    stbuf->st_gid = ext.st_gid;
    //debug_output_wsl_extend_attr(p, &ext);
    return 0;
}

static int wsl_access(const char *path, int mask)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = access(p, mask);
    if (res == -1)
        return -errno;

    return 0;
}

static int wsl_readlink(const char *path, char *buf, size_t size)
{
    //int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    //res = readlink(p, buf, size - 1);
    //if (res == -1)
    //    return -errno;
    //char buf[MAX_PATHLEN];
    FILE *file;
    size_t nread;

    file = fopen(p, "r");
    if (file) {
        if ((nread = fread(buf, 1, size, file)) > 0) {
            //fwrite(buf, 1, nread, stdout);
        }
        if (ferror(file)) {
            /* deal with error */
        }
        fclose(file);
    }
    //buf[res] = '\0';
    return 0;
}

static int wsl_opendir(const char *path, struct fuse_file_info *fi)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    struct wsl_dirp *dir = (struct wsl_dirp*) malloc(sizeof(struct wsl_dirp));
    if (dir == NULL)
        return -ENOMEM;

    dir->dp = opendir(p);
    if (dir->dp == NULL) {
        res = -errno;
        free(dir);
        return res;
    }
    dir->offset = 0;
    dir->entry = NULL;
    fi->fh = (unsigned long) dir;
    return 0;
}

static int wsl_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi)
{
    struct wsl_dirp *d = (struct wsl_dirp*) fi->fh;
    char p[MAX_PATHLEN];
    wsl_extended_attr_v1_t ext;
    int res;
    (void) path;
    if (offset != d->offset) {
        seekdir(d->dp, offset);
        d->entry = NULL;
        d->offset = offset;
    }

    while ((d->entry = readdir(d->dp)) != NULL) {
        sanitize_path_with_dir(d->entry->d_name, path, p);
        res = get_extended_attr(p, &ext);

        struct stat st;
        off_t nextoff;
        memset(&st, 0, sizeof(st));
        st.st_ino = d->entry->d_ino;
        st.st_mode = d->entry->d_type << 12;
        nextoff = telldir(d->dp);
        if (res != -1 && ext.st_mode!=0) {
            if (filler(buf, d->entry->d_name, &st, nextoff)) {
                break;
            }
        }
        d->entry = NULL;
        d->offset = nextoff;
    }

    return 0;
}

static int wsl_releasedir(const char *path, struct fuse_file_info *fi)
{
    struct wsl_dirp *d = (struct wsl_dirp*) fi->fh;
    (void) path;
    closedir(d->dp);
    free(d);
    return 0;
}

// Not yet implimented
static int wsl_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;
    char p[MAX_PATHLEN];
    char dupP[MAX_PATHLEN];
    struct stat stbuf;
    wsl_extended_attr_v1_t base_ext;
    sanitize_path(path, p);
    sanitize_path(path, dupP);

    /* On Linux this could just be 'mknod(path, mode, rdev)' but this
       is more portable */
    if (S_ISREG(mode)) {
        res = open(p, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0)
            res = close(res);
    } else if (S_ISFIFO(mode)) {
        //res = mkfifo(p, mode);
        return -ENOTSUP;
    } else {
        //res = mknod(p, mode, rdev);
        return -ENOTSUP;
    }
    if (res == -1)
        return -errno;
    
    res = lstat(p, &stbuf);
    if (res == -1)
        return -errno;

    char * baseP = dirname(dupP);
    res = get_extended_attr(baseP, &base_ext);
    
    if (res == -1)
        return -errno;
    
    wsl_extended_attr_v1_t ext = {
        .pad1 = 0x00000048,
        .pad2 = 0x00380700,
        .pad3 = 0x5441584c,  
        .pad4 = 0x00425254,  
        .Flags = WSL_DEFAULT_FLAGS,
        .Version = WSL_VERSION,
        .st_mode = base_ext.st_mode,
        .st_uid = base_ext.st_uid,
        .st_gid = base_ext.st_gid,
        .st_rdev = 0,
        .st_atime_nsec = 0,
        .st_mtime_nsec = 0,
        .st_ctime_nsec = 0,
        .st_at = stbuf.st_atime,
        .st_mt = stbuf.st_mtime,
        .st_ct = stbuf.st_ctime
    };
    
    res = set_extended_attr(p, &ext);
    if (res == -1)
        return -errno;

    return 0;
}

static int wsl_mkdir(const char *path, mode_t mode)
{
    int res;
    char p[MAX_PATHLEN];
    char dupP[MAX_PATHLEN];
    struct stat stbuf;
    wsl_extended_attr_v1_t base_ext;
    sanitize_path(path, p);
    sanitize_path(path, dupP);

    res = mkdir(p, mode);
    if (res == -1)
        return -errno;
   
    res = lstat(p, &stbuf);
    if (res == -1)
        return -errno;

    char * baseP = dirname(dupP);
    res = get_extended_attr(baseP, &base_ext);
    
    if (res == -1)
        return -errno;
    
    wsl_extended_attr_v1_t ext = {
        .pad1 = 0x00000048,
        .pad2 = 0x00380700,
        .pad3 = 0x5441584c,  
        .pad4 = 0x00425254,  
        .Flags = WSL_DEFAULT_FLAGS,
        .Version = WSL_VERSION,
        .st_mode = base_ext.st_mode,
        .st_uid = base_ext.st_uid,
        .st_gid = base_ext.st_gid,
        .st_rdev = 0,
        .st_atime_nsec = 0,
        .st_mtime_nsec = 0,
        .st_ctime_nsec = 0,
        .st_at = stbuf.st_atime,
        .st_mt = stbuf.st_mtime,
        .st_ct = stbuf.st_ctime
    };
    
    res = set_extended_attr(p, &ext);
    if (res == -1)
        return -errno;

    return 0;
}

static int wsl_unlink(const char *path)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = unlink(p);
    if (res == -1)
        return -errno;

    return 0;
}

static int wsl_rmdir(const char *path)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = rmdir(p);
    if (res == -1)
        return -errno;

    return 0;
}

static int wsl_symlink(const char *from, const char *to)
{
    int res;
    int error;
    int fd;
    //char f[MAX_PATHLEN];
    char t[MAX_PATHLEN];
    char dupP[MAX_PATHLEN];
    struct stat stbuf;
    wsl_extended_attr_v1_t base_ext;
    //sanitize_path(from, f);
    sanitize_path(to, t);
    sanitize_path(to, dupP);
    fd = creat(t, S_IRWXO | S_IRWXG | S_IRWXU | S_IFREG);
    if (fd == -1)
        return -errno;

    if (write(fd, from, strlen(t))==-1) {
        error = errno;
        close(fd);
        remove(t); 
        return -error;
    }
    close(fd);

    res = lstat(t, &stbuf);
    if (res == -1)
        return -errno;

    char * baseP = dirname(dupP);
    res = get_extended_attr(baseP, &base_ext);
    //debug_output_wsl_extend_attr(baseP, &base_ext);
    if (res == -1)
        return -errno;
    
    wsl_extended_attr_v1_t ext = {
        .pad1 = 0x00000048,
        .pad2 = 0x00380700,
        .pad3 = 0x5441584c,  
        .pad4 = 0x00425254,  
        .Flags = WSL_DEFAULT_FLAGS,
        .Version = WSL_VERSION,
        .st_mode = S_IRWXO | S_IRWXG | S_IRWXU | S_IFLNK,
        .st_uid = base_ext.st_uid,
        .st_gid = base_ext.st_gid,
        .st_rdev = 0,
        .st_atime_nsec = 0,
        .st_mtime_nsec = 0,
        .st_ctime_nsec = 0,
        .st_at = stbuf.st_atime,
        .st_mt = stbuf.st_mtime,
        .st_ct = stbuf.st_ctime
    };
    //debug_output_wsl_extend_attr(t, &ext);
    res = set_extended_attr(t, &ext);
    if (res == -1)
        return -errno;

    return 0;
}

static int wsl_rename(const char *from, const char *to, unsigned int flags)
{
    int res;
    char f[MAX_PATHLEN];
    char t[MAX_PATHLEN];
    sanitize_path(from, f);
    sanitize_path(to, t);

    if (flags)
        return -EINVAL;

    res = rename(f, t);
    if (res == -1)
        return -errno;

    return 0;
}

// Not yet implimented
static int wsl_link(const char *from, const char *to)
{
    int res;
    char f[MAX_PATHLEN];
    char t[MAX_PATHLEN];
    sanitize_path(from, f);
    sanitize_path(to, t);

    res = link(f, t);
    if (res == -1)
        return -errno;

    return 0;
}

static int wsl_chmod(const char *path, mode_t mode)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    //res = chmod(p, mode);
    //if (res == -1)
    //    return -errno;
    wsl_extended_attr_v1_t ext;
    res = get_extended_attr(p, &ext);
    if (res == -1)
        return -errno;
    ext.st_mode = mode;
    res = set_extended_attr(p, &ext);
    if (res == -1)
        return -errno;

    return 0;
}

static int wsl_chown(const char *path, uid_t uid, gid_t gid)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    //res = lchown(p, uid, gid);
    //if (res == -1)
    //    return -errno;
    wsl_extended_attr_v1_t ext;
    res = get_extended_attr(p, &ext);
    if (res == -1)
        return -errno;
    ext.st_uid = uid;
    ext.st_gid = gid;
    res = set_extended_attr(p, &ext);
    if (res == -1)
        return -errno;


    return 0;
}

static int wsl_truncate(const char *path, off_t size)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = truncate(p, size);
    if (res == -1)
        return -errno;

    return 0;
}

//static int wsl_ftruncate(const char* path, off_t size, struct fuse_file_info *fi)
//{
//    int res;
//    (void) path;
//    res = ftruncate(fi->fh, size);
//    if (res == -1)
//        return -errno;
//
//    return 0;
//}

#ifdef HAVE_UTIMENSAT
static int wsl_utimens(const char *path, const struct timespec ts[2])
{
    int res;
    char p[MAX_PATHLEN];
    struct stat stbuf;
    sanitize_path(path, p);
    
    /* don't use utime/utimes since they follow symlinks */
    res = utimensat(0, p, ts, AT_SYMLINK_NOFOLLOW);
    if (res == -1)
        return -errno;
    
    res = lstat(p, &stbuf);
    if (res == -1)
        return -errno;
    
    wsl_extended_attr_v1_t ext;
    res = get_extended_attr(p, &ext);
    if (res == -1)
        return -errno;

    ext.st_at = stbuf.st_atime;
    ext.st_mt = stbuf.st_mtime;
    ext.st_ct = stbuf.st_ctime;
    res = set_extended_attr(p, &ext);
    if (res == -1)
        return -errno;

    return 0;
}
#endif

static int wsl_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{

    int res;
    char p[MAX_PATHLEN];
    char dupP[MAX_PATHLEN];
    struct stat stbuf;
    sanitize_path(path, p);
    sanitize_path(path, dupP);
    wsl_extended_attr_v1_t base_ext;

    res = open(p, fi->flags, mode);
    if (res == -1)
        return -errno;

    fi->fh = res;

    res = lstat(p, &stbuf);
    if (res == -1)
        return -errno;

    char * baseP = dirname(dupP);
    res = get_extended_attr(baseP, &base_ext);
    
    if (res == -1)
        return -errno;
    
    wsl_extended_attr_v1_t ext = {
        .pad1 = 0x00000048,
        .pad2 = 0x00380700,
        .pad3 = 0x5441584c,  
        .pad4 = 0x00425254,  
        .Flags = WSL_DEFAULT_FLAGS,
        .Version = WSL_VERSION,
        .st_mode = mode,
        .st_uid = base_ext.st_uid,
        .st_gid = base_ext.st_gid,
        .st_rdev = 0,
        .st_atime_nsec = 0,
        .st_mtime_nsec = 0,
        .st_ctime_nsec = 0,
        .st_at = stbuf.st_atime,
        .st_mt = stbuf.st_mtime,
        .st_ct = stbuf.st_ctime
    };
    res = set_extended_attr(p, &ext);
    if (res == -1)
        return -errno;

    return 0;
}

static int wsl_open(const char *path, struct fuse_file_info *fi)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = open(p, fi->flags);
    if (res == -1)
        return -errno;

    fi->fh = res;
    return 0;
}

static int wsl_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi)
{
    int res;

    (void) path;
    res = pread(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

static int wsl_write(const char *path, const char *buf, size_t size,
        off_t offset, struct fuse_file_info *fi)
{
    int res;

    (void) path;
    res = pwrite(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

static int wsl_statfs(const char *path, struct statvfs *stbuf)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = statvfs(p, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int wsl_flush(const char* path, struct fuse_file_info *fi)
{
    int res;
    (void) path;

    res = close(dup(fi->fh));
    if (res == -1)
        return -errno;

    return 0;
}

static int wsl_release(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    close(fi->fh);

    return 0;
}

static int wsl_fsync(const char *path, int isdatasync,
        struct fuse_file_info *fi)
{
    int res;
    (void) path;
    (void) isdatasync;

    res = fsync(fi->fh);
    if (res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int wsl_fallocate(const char *path, int mode,
        off_t offset, off_t length, struct fuse_file_info *fi)
{

    (void) path;

    if (mode)
        return -EOPNOTSUPP;

    res = -posix_fallocate(fi->fh, offset, length);
    return res;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int wsl_setxattr(const char *path, const char *name, const char *value,
        size_t size, int flags)
{
    char p[MAX_PATHLEN];
    sanitize_path(path, p);
    int res = lsetxattr(p, name, value, size, flags);
    if (res == -1)
        return -errno;
    return 0;
}

static int wsl_getxattr(const char *path, const char *name, char *value,
        size_t size)
{
    char p[MAX_PATHLEN];
    sanitize_path(path, p);
    int res = lgetxattr(p, name, value, size);
    if (res == -1)
        return -errno;
    return res;
}

static int wsl_listxattr(const char *path, char *list, size_t size)
{
    char p[MAX_PATHLEN];
    sanitize_path(path, p);
    int res = llistxattr(p, list, size);
    if (res == -1)
        return -errno;
    return res;
}

static int wsl_removexattr(const char *path, const char *name)
{
    char p[MAX_PATHLEN];
    sanitize_path(path, p);
    int res = lremovexattr(p, name);
    if (res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */

static int wsl_flock(const char* path, struct fuse_file_info *fi, int op)
{
    int res;
    (void) path;

    res = flock(fi->fh, op);
    if (res == -1)
        return -errno;

    return 0;
}

static struct fuse_operations wsl_oper = {
    .getattr	= wsl_getattr,
    .access		= wsl_access,
    .readlink	= wsl_readlink,
    .opendir    = wsl_opendir,
    .releasedir = wsl_releasedir,
    .readdir	= wsl_readdir,
    .mknod		= wsl_mknod, // Not yet implimented
    .mkdir		= wsl_mkdir,
    .symlink	= wsl_symlink,
    .unlink		= wsl_unlink,
    .rmdir		= wsl_rmdir,
    .rename		= wsl_rename,
    .link		= wsl_link, // Not yet implimented
    .chmod		= wsl_chmod,
    .chown		= wsl_chown,
    .truncate	= wsl_truncate,
#ifdef HAVE_UTIMENSAT
    .utimens	= wsl_utimens,
#endif
    .create     = wsl_create,
    .open		= wsl_open,
    .read		= wsl_read,
    .write		= wsl_write,
    .statfs		= wsl_statfs,
    .flush      = wsl_flush,
    .release	= wsl_release,
    .fsync		= wsl_fsync,
#ifdef HAVE_POSIX_FALLOCATE
    .fallocate	= wsl_fallocate,
#endif
#ifdef HAVE_SETXATTR
    .setxattr	= wsl_setxattr,
    .getxattr	= wsl_getxattr,
    .listxattr	= wsl_listxattr,
    .removexattr	= wsl_removexattr,
#endif
    .flock = wsl_flock,
};


struct wsl_opt_struct {
    unsigned long val;
    char * str;
} wsl_opts;


/*
 * option parsing callback
 * return -1 indicates an error
 * return 0 accepts the parameter
 * return 1 retain the parameter to fuse
 */
int wsl_opt_proc(void *data, const char* arg, int key, struct fuse_args *outargs)
{
    (void)(data);
    (void)(outargs);
    switch(key)
    {
        case FUSE_OPT_KEY_NONOPT:
            if (!root) {
                root = NULL;
                root = realpath(arg, root);
                rootLen = strlen(root);
                return 0;
            }
            return 1;
        default: /* else pass to fuse */
            return 1;
    }
}


int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    int res;

    //TODO:
    wsl_opts.val = 0;
    wsl_opts.str = NULL;

    if (fuse_opt_parse(&args, &wsl_opts, NULL, wsl_opt_proc) == -1) {
        perror("error on fuse_opt_parse");
        exit(1);
    }
    else {
        printf("arguments to fuse_main: ");
        for (int i=0; i < args.argc; i++) {
            printf("%s ", args.argv[i]);
        }
        printf("\n");
        printf("Demo parameters in wsl_opts: val= %lu, str=", wsl_opts.val);
        if (wsl_opts.str) {
            printf(" %s\n", wsl_opts.str);
        }
        else {
            printf(" NULL\n");
        }
        if (root) {
            printf("root: %s\n", root);
        }
        else {
            printf("no root!\n");
        }
    }

    umask(0);
    res = fuse_main(args.argc, args.argv, &wsl_oper, NULL);

    fuse_opt_free_args(&args);
    if (root) {
        free(root);
    }

    return res;
}
