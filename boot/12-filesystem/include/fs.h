#ifndef FS_H
#define FS_H

#include "types.h"

#define MAX_FDS         16
#define MAX_PATH        256

/* File access flags */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_ACCMODE   0x0003
#define O_CREAT     0x0200
#define O_TRUNC     0x0400

/* Simple file info structure */
typedef struct {
    char name[32];
    uint32_t size;
} file_info_t;

/* Filesystem Operations */
struct vfs_operations {
    int (*lookup)(const char *name);
    int (*read)(int file_index, uint32_t offset, void *buf, uint32_t len);
    int (*get_file_count)(void);
    int (*get_file_info)(int index, char *name_out, uint32_t *size_out);
    
    /* Write operations */
    int (*create)(const char *name);
    int (*write)(int file_index, uint32_t offset, const void *buf, uint32_t len);
    int (*truncate)(int file_index, uint32_t new_size);
};

/* VFS API */
void vfs_init(void);
int vfs_mount(const char *mount_point, struct vfs_operations *fs_ops);

int vfs_open(const char *path, int flags);
int vfs_read(int fd, void *buf, uint32_t len);
int vfs_write(int fd, const void *buf, uint32_t len);
int vfs_close(int fd);
int vfs_listdir(const char *path, void *entries, uint32_t max_entries);

#endif /* FS_H */
