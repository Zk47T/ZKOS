#include "fs.h"
#include "uart.h"
#include "string.h"

#define E_FAIL  -1
#define E_NOENT -2
#define E_BADF  -3
#define E_MFILE -4
#define E_ARG   -5
#define E_PERM  -6

/* File Descriptor Table */
struct vfs_fd {
    bool     in_use;
    uint32_t file_index;
    uint32_t offset;
    int      flags;
};

static struct vfs_fd fd_table[MAX_FDS];

/* Mount Table (only root / supported for now) */
static struct vfs_operations *root_fs = NULL;

void vfs_init(void)
{
    uart_puts("[VFS] Initializing VFS...\r\n");
    for (int i = 0; i < MAX_FDS; i++) {
        fd_table[i].in_use = false;
    }
}

int vfs_mount(const char *mount_point, struct vfs_operations *fs_ops)
{
    if (strcmp(mount_point, "/") == 0) {
        root_fs = fs_ops;
        uart_puts("[VFS] Mounted root filesystem\r\n");
        return 0;
    }
    return E_FAIL;
}

int vfs_open(const char *path, int flags)
{
    if (!root_fs) return E_NOENT;

    int access = flags & O_ACCMODE;
    if (access != O_RDONLY && access != O_WRONLY && access != O_RDWR) return E_ARG;
    if ((access == O_WRONLY || access == O_RDWR) && root_fs->write == NULL) return E_PERM;

    const char *filename = path;
    if (*filename == '/') filename++;

    int file_index = root_fs->lookup(filename);

    if (file_index < 0) {
        if ((flags & O_CREAT) && root_fs->create != NULL) {
            file_index = root_fs->create(filename);
            if (file_index < 0) return file_index;
        } else {
            return E_NOENT;
        }
    } else if (flags & O_TRUNC) {
        if (root_fs->truncate != NULL) {
            root_fs->truncate(file_index, 0);
        }
    }

    int fd = -1;
    for (int i = 3; i < MAX_FDS; i++) { // reserve 0,1,2 for stdio
        if (!fd_table[i].in_use) {
            fd = i; break;
        }
    }
    if (fd < 0) return E_MFILE;

    fd_table[fd].in_use = true;
    fd_table[fd].file_index = file_index;
    fd_table[fd].offset = 0;
    fd_table[fd].flags = flags;

    return fd;
}

int vfs_read(int fd, void *buf, uint32_t len)
{
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) return E_BADF;
    if (!root_fs) return E_FAIL;

    int bytes = root_fs->read(fd_table[fd].file_index, fd_table[fd].offset, buf, len);
    if (bytes > 0) fd_table[fd].offset += bytes;
    
    return bytes;
}

int vfs_write(int fd, const void *buf, uint32_t len)
{
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) return E_BADF;
    if (!root_fs || !root_fs->write) return E_PERM;

    int bytes = root_fs->write(fd_table[fd].file_index, fd_table[fd].offset, buf, len);
    if (bytes > 0) fd_table[fd].offset += bytes;

    return bytes;
}

int vfs_close(int fd)
{
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) return E_BADF;
    fd_table[fd].in_use = false;
    return 0;
}

int vfs_listdir(const char *path, void *entries, uint32_t max_entries)
{
    if (!root_fs) return E_NOENT;
    if (strcmp(path, "/") != 0) return E_NOENT;

    int count = root_fs->get_file_count();
    if (count < 0) return count;

    file_info_t *arr = (file_info_t *)entries;
    int filled = 0;
    for (int i = 0; i < count && filled < (int)max_entries; i++) {
        if (root_fs->get_file_info(i, arr[filled].name, &arr[filled].size) == 0) {
            filled++;
        }
    }
    return filled;
}
