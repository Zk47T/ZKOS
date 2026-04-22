#include "fs.h"
#include "string.h"
#include "mm.h"

#define MAX_RAMFS_FILES 10
#define MAX_FILE_SIZE   4096

typedef struct {
    bool in_use;
    char name[32];
    uint8_t *data;
    uint32_t size;
    uint32_t capacity;
    bool read_only;
} ramfs_file_t;

static ramfs_file_t ramfs_files[MAX_RAMFS_FILES];

/* Static embedded files */
static const char str_hello[] = "Hello from ZKOS!\nThis file is served by RAMFS.\n";
static const char str_readme[] = "ZKOS v0.12 - Bare-metal OS for i.MX93\nAuthor: Nguyen Minh Tien\n";
static const char str_version[] = "0.12.0\n";
static const char str_cpu[] = "Cortex-A55 @ i.MX93\nException Level: EL2\n";

static void create_ro_file(int idx, const char *name, const char *content)
{
    ramfs_files[idx].in_use = true;
    strcpy(ramfs_files[idx].name, name);
    ramfs_files[idx].data = (uint8_t *)content;
    ramfs_files[idx].size = strlen(content);
    ramfs_files[idx].capacity = ramfs_files[idx].size;
    ramfs_files[idx].read_only = true;
}

static int ramfs_lookup(const char *name)
{
    for (int i = 0; i < MAX_RAMFS_FILES; i++) {
        if (ramfs_files[i].in_use && strcmp(ramfs_files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int ramfs_read(int file_index, uint32_t offset, void *buf, uint32_t len)
{
    if (file_index < 0 || file_index >= MAX_RAMFS_FILES || !ramfs_files[file_index].in_use) return -1;
    ramfs_file_t *f = &ramfs_files[file_index];

    if (offset >= f->size) return 0; // EOF

    uint32_t avail = f->size - offset;
    uint32_t to_read = (len < avail) ? len : avail;

    memcpy(buf, f->data + offset, to_read);
    return to_read;
}

static int ramfs_get_file_count(void)
{
    int c = 0;
    for (int i = 0; i < MAX_RAMFS_FILES; i++) {
        if (ramfs_files[i].in_use) c++;
    }
    return c;
}

static int ramfs_get_file_info(int index, char *name_out, uint32_t *size_out)
{
    int c = 0;
    for (int i = 0; i < MAX_RAMFS_FILES; i++) {
        if (ramfs_files[i].in_use) {
            if (c == index) {
                strcpy(name_out, ramfs_files[i].name);
                *size_out = ramfs_files[i].size;
                return 0;
            }
            c++;
        }
    }
    return -1;
}

static int ramfs_create(const char *name)
{
    for (int i = 0; i < MAX_RAMFS_FILES; i++) {
        if (!ramfs_files[i].in_use) {
            ramfs_files[i].in_use = true;
            strcpy(ramfs_files[i].name, name);
            
            ramfs_files[i].data = kmalloc(MAX_FILE_SIZE);
            if (!ramfs_files[i].data) {
                ramfs_files[i].in_use = false;
                return -1; // Out of memory
            }
            ramfs_files[i].size = 0;
            ramfs_files[i].capacity = MAX_FILE_SIZE;
            ramfs_files[i].read_only = false;
            return i;
        }
    }
    return -1;
}

static int ramfs_write(int file_index, uint32_t offset, const void *buf, uint32_t len)
{
    if (file_index < 0 || file_index >= MAX_RAMFS_FILES || !ramfs_files[file_index].in_use) return -1;
    ramfs_file_t *f = &ramfs_files[file_index];

    if (f->read_only) return -1;
    if (offset >= f->capacity) return -1;

    uint32_t avail = f->capacity - offset;
    uint32_t to_write = (len < avail) ? len : avail;

    memcpy(f->data + offset, buf, to_write);
    
    if (offset + to_write > f->size) {
        f->size = offset + to_write;
    }

    return to_write;
}

static int ramfs_truncate(int file_index, uint32_t new_size)
{
    if (file_index < 0 || file_index >= MAX_RAMFS_FILES || !ramfs_files[file_index].in_use) return -1;
    ramfs_file_t *f = &ramfs_files[file_index];

    if (f->read_only) return -1;
    
    if (new_size <= f->capacity) {
        f->size = new_size;
        return 0;
    }
    return -1;
}

struct vfs_operations ramfs_ops = {
    .lookup = ramfs_lookup,
    .read = ramfs_read,
    .get_file_count = ramfs_get_file_count,
    .get_file_info = ramfs_get_file_info,
    .create = ramfs_create,
    .write = ramfs_write,
    .truncate = ramfs_truncate
};

void ramfs_init(void)
{
    for (int i = 0; i < MAX_RAMFS_FILES; i++) {
        ramfs_files[i].in_use = false;
    }

    create_ro_file(0, "hello.txt", str_hello);
    create_ro_file(1, "readme.txt", str_readme);
    create_ro_file(2, "version", str_version);
    create_ro_file(3, "cpuinfo", str_cpu);

    vfs_mount("/", &ramfs_ops);
}
