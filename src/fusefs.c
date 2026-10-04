#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <time.h>

static char storage_root[PATH_MAX];

/*
 * Convert a FuseFS path such as:
 *
 * /hello.txt
 *
 * into the actual backend path:
 *
 * /root/FuseFS/storage/hello.txt
 */
static void get_storage_path(const char *path, char *fullpath)
{
    snprintf(fullpath, PATH_MAX, "%s%s", storage_root, path);
}


/* ---------------- GETATTR ---------------- */

static int fusefs_getattr(const char *path,
                          struct stat *stbuf,
                          struct fuse_file_info *fi)
{
    (void) fi;

    char fullpath[PATH_MAX];

    get_storage_path(path, fullpath);

    if (lstat(fullpath, stbuf) == -1)
        return -errno;

    return 0;
}


/* ---------------- READ DIRECTORY ---------------- */

static int fusefs_readdir(const char *path,
                          void *buf,
                          fuse_fill_dir_t filler,
                          off_t offset,
                          struct fuse_file_info *fi,
                          enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
    (void) flags;

    char fullpath[PATH_MAX];

    get_storage_path(path, fullpath);

    DIR *dp = opendir(fullpath);

    if (dp == NULL)
        return -errno;

    struct dirent *entry;

    while ((entry = readdir(dp)) != NULL)
    {
        struct stat st;

        memset(&st, 0, sizeof(st));

        st.st_ino = entry->d_ino;
        st.st_mode = entry->d_type << 12;

        if (filler(buf, entry->d_name, &st, 0, 0))
            break;
    }

    closedir(dp);

    return 0;
}


/* ---------------- OPEN FILE ---------------- */

static int fusefs_open(const char *path,
                       struct fuse_file_info *fi)
{
    char fullpath[PATH_MAX];

    get_storage_path(path, fullpath);

    int fd = open(fullpath, fi->flags);

    if (fd == -1)
        return -errno;

    close(fd);

    return 0;
}


/* ---------------- READ FILE ---------------- */

static int fusefs_read(const char *path,
                       char *buf,
                       size_t size,
                       off_t offset,
                       struct fuse_file_info *fi)
{
    (void) fi;

    char fullpath[PATH_MAX];

    get_storage_path(path, fullpath);

    int fd = open(fullpath, O_RDONLY);

    if (fd == -1)
        return -errno;

    ssize_t result = pread(fd, buf, size, offset);

    if (result == -1)
        result = -errno;

    close(fd);

    return result;
}


/* ---------------- WRITE FILE ---------------- */

static int fusefs_write(const char *path,
                        const char *buf,
                        size_t size,
                        off_t offset,
                        struct fuse_file_info *fi)
{
    (void) fi;

    char fullpath[PATH_MAX];

    get_storage_path(path, fullpath);

    int fd = open(fullpath, O_WRONLY);

    if (fd == -1)
        return -errno;

    ssize_t result = pwrite(fd, buf, size, offset);

    if (result == -1)
        result = -errno;

    close(fd);

    return result;
}


/* ---------------- CREATE FILE ---------------- */

static int fusefs_create(const char *path,
                         mode_t mode,
                         struct fuse_file_info *fi)
{
    char fullpath[PATH_MAX];

    get_storage_path(path, fullpath);

    int fd = open(fullpath,
                  fi->flags | O_CREAT,
                  mode);

    if (fd == -1)
        return -errno;

    close(fd);

    return 0;
}


/* ---------------- MAKE DIRECTORY ---------------- */

static int fusefs_mkdir(const char *path,
                        mode_t mode)
{
    char fullpath[PATH_MAX];

    get_storage_path(path, fullpath);

    if (mkdir(fullpath, mode) == -1)
        return -errno;

    return 0;
}


/* ---------------- DELETE FILE ---------------- */

static int fusefs_unlink(const char *path)
{
    char fullpath[PATH_MAX];

    get_storage_path(path, fullpath);

    if (unlink(fullpath) == -1)
        return -errno;

    return 0;
}


/* ---------------- DELETE DIRECTORY ---------------- */

static int fusefs_rmdir(const char *path)
{
    char fullpath[PATH_MAX];

    get_storage_path(path, fullpath);

    if (rmdir(fullpath) == -1)
        return -errno;

    return 0;
}


/* ---------------- CHANGE FILE SIZE ---------------- */

static int fusefs_truncate(const char *path,
                           off_t size,
                           struct fuse_file_info *fi)
{
    (void) fi;

    char fullpath[PATH_MAX];

    get_storage_path(path, fullpath);

    if (truncate(fullpath, size) == -1)
        return -errno;

    return 0;
}


/* ---------------- CHANGE FILE TIMESTAMP ---------------- */

/*
 * This function is required by commands such as:
 *
 * touch hello.txt
 *
 * touch creates the file and then updates its timestamps.
 */
static int fusefs_utimens(const char *path,
                          const struct timespec tv[2],
                          struct fuse_file_info *fi)
{
    (void) fi;

    char fullpath[PATH_MAX];

    get_storage_path(path, fullpath);

    if (utimensat(AT_FDCWD,
                  fullpath,
                  tv,
                  0) == -1)
    {
        return -errno;
    }

    return 0;
}


/* ---------------- FUSE OPERATIONS ---------------- */

static struct fuse_operations fusefs_operations =
{
    .getattr  = fusefs_getattr,
    .readdir  = fusefs_readdir,
    .open     = fusefs_open,
    .read     = fusefs_read,
    .write    = fusefs_write,
    .create   = fusefs_create,
    .mkdir    = fusefs_mkdir,
    .unlink   = fusefs_unlink,
    .rmdir    = fusefs_rmdir,
    .truncate = fusefs_truncate,
    .utimens  = fusefs_utimens
};


/* ---------------- MAIN ---------------- */

int main(int argc, char *argv[])
{
    /*
     * Find the backend storage directory.
     *
     * FuseFS should be started from:
     *
     * ~/FuseFS
     */
    if (realpath("storage", storage_root) == NULL)
    {
        perror("realpath(storage)");

        return 1;
    }

    printf("FuseFS backend storage: %s\n",
           storage_root);

    return fuse_main(argc,
                     argv,
                     &fusefs_operations,
                     NULL);
}
