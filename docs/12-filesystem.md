# Bài 12 — Filesystem: "Everything is a File"

Triết lý UNIX nổi tiếng nhất là **"everything is a file"** — UART, terminal, socket, disk đều đọc/ghi qua interface `open/read/write/close` giống nhau. Bài này hiện thực hóa điều đó trong ZKOS: xây dựng **VFS** (Virtual Filesystem) và **RAMFS** (filesystem hoàn toàn trong RAM).

---

## Bối cảnh: Tại sao cần VFS abstraction?

Không có VFS, mỗi loại storage cần API riêng: `fat32_open()`, `ramfs_open()`, `uart_read()`... Code gọi phải biết đang dùng loại nào. Với VFS, tất cả đều là `vfs_open()` — kernel gọi qua một bảng hàm ảo (function pointer table), giống virtual dispatch trong C++.

```
User: vfs_open("hello.txt")
         │
         ▼
      VFS layer (fs.c)
         │ mount table: "/" → &ramfs_ops
         │
         ▼
      RAMFS ops: ramfs_lookup("hello.txt") → file index 0
         │
         ▼
      File Descriptor: fd=3, ops=&ramfs_ops, file_index=0

User: vfs_read(fd=3, buf, 100)
         │
         ▼
      VFS → fd_table[3].ops->read(fd_table[3].file_index, 0, buf, 100)
         │
         ▼
      RAMFS: copy từ ramfs_files[0].data → buf
```

---

## VFS Operations — Bảng hàm ảo

[boot/12-filesystem/include/fs.h](../boot/12-filesystem/include/fs.h) định nghĩa interface:

```c
struct vfs_operations {
    int  (*lookup)(const char *name);              // tìm file, trả về index
    int  (*read)(int idx, uint32_t off, void *buf, uint32_t len);
    int  (*write)(int idx, uint32_t off, const void *buf, uint32_t len);
    int  (*create)(const char *name);              // tạo file mới
    int  (*truncate)(int idx, uint32_t new_size);
    int  (*get_file_count)(void);
    int  (*get_file_info)(int n, char *name, uint32_t *size);
};
```

Mỗi filesystem (RAMFS, FAT32 trong tương lai) cung cấp một `struct vfs_operations` với con trỏ hàm thực. VFS layer chỉ gọi qua con trỏ — không cần biết đang dùng filesystem gì.

**Mount table:**

```c
// boot/12-filesystem/kernel/fs.c
#define MAX_MOUNTS 4

struct mount_entry {
    char path[32];
    struct vfs_operations *ops;
};
static struct mount_entry mount_table[MAX_MOUNTS];

void vfs_mount(const char *path, struct vfs_operations *ops)
{
    // Đăng ký filesystem tại mount point
    strcpy(mount_table[mount_count].path, path);
    mount_table[mount_count].ops = ops;
    mount_count++;
}
```

---

## RAMFS — Filesystem trong RAM

[boot/12-filesystem/kernel/ramfs.c](../boot/12-filesystem/kernel/ramfs.c):

```c
#define MAX_RAMFS_FILES  10
#define MAX_FILE_SIZE    4096  // 4KB mỗi file dynamic

typedef struct {
    bool in_use;
    char name[32];
    uint8_t *data;   // con trỏ đến nội dung (static string hoặc kmalloc)
    uint32_t size;
    uint32_t capacity;
    bool read_only;  // file nhúng vào kernel image → không ghi được
} ramfs_file_t;

static ramfs_file_t ramfs_files[MAX_RAMFS_FILES];
```

**Hai loại file trong RAMFS:**

1. **Read-only (embedded):** Nội dung là `const char[]` nằm sẵn trong `.rodata` của kernel binary. `data` pointer trỏ thẳng vào đó. Không tốn RAM động.

```c
static const char str_hello[] = "Hello from ZKOS!\nThis file is served by RAMFS.\n";
static const char str_cpu[]   = "Cortex-A55 @ i.MX93\nException Level: EL2\n";

// Tạo file read-only, data pointer trỏ vào string literal
create_ro_file(0, "hello.txt", str_hello);
create_ro_file(3, "cpuinfo",   str_cpu);
```

2. **Writable (dynamic):** Dùng `kmalloc(MAX_FILE_SIZE)` từ bài 8 để cấp phát 4KB buffer. File có thể ghi/truncate.

```c
static int ramfs_create(const char *name)
{
    // Tìm slot trống
    for (int i = 0; i < MAX_RAMFS_FILES; i++) {
        if (!ramfs_files[i].in_use) {
            ramfs_files[i].data = kmalloc(MAX_FILE_SIZE);  // 4KB từ heap
            if (!ramfs_files[i].data) return -1;           // OOM
            ramfs_files[i].read_only = false;
            ramfs_files[i].size = 0;
            ...
            return i;
        }
    }
    return -1;  // Đầy (10 file max)
}
```

---

## File Descriptor Table

[boot/12-filesystem/kernel/fs.c](../boot/12-filesystem/kernel/fs.c):

```c
#define MAX_FDS  16

struct fd_entry {
    bool in_use;
    struct vfs_operations *ops;  // filesystem nào đang serve file này
    int file_index;              // index trong filesystem đó
    uint32_t offset;             // vị trí đọc hiện tại (như file cursor)
};
static struct fd_entry fd_table[MAX_FDS];
```

**FD 0, 1, 2 reserved:** stdin, stdout, stderr — convention UNIX. `vfs_open()` bắt đầu từ FD 3.

```c
int vfs_open(const char *path)
{
    // 1. Tìm mount point phù hợp
    struct vfs_operations *ops = find_ops_for_path(path);
    if (!ops) return -1;

    // 2. Lookup file trong filesystem
    int file_idx = ops->lookup(path);
    if (file_idx < 0) return -1;

    // 3. Cấp phát FD (bắt đầu từ 3)
    for (int fd = 3; fd < MAX_FDS; fd++) {
        if (!fd_table[fd].in_use) {
            fd_table[fd].in_use = true;
            fd_table[fd].ops = ops;
            fd_table[fd].file_index = file_idx;
            fd_table[fd].offset = 0;
            return fd;
        }
    }
    return -1;  // Hết FD
}

int vfs_read(int fd, void *buf, uint32_t len)
{
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) return -1;
    struct fd_entry *f = &fd_table[fd];

    int n = f->ops->read(f->file_index, f->offset, buf, len);
    if (n > 0) f->offset += n;  // Tự động advance cursor
    return n;
}
```

---

## Shell Commands mới: ls, cat, write

```c
// boot/12-filesystem/main.c
else if (strcmp(cmd, "ls") == 0) {
    // List tất cả file trong RAMFS
    int count = ramfs_ops.get_file_count();
    for (int i = 0; i < count; i++) {
        char name[32]; uint32_t size;
        ramfs_ops.get_file_info(i, name, &size);
        uart_puts(name);
        uart_puts("  (");
        uart_putint(size);
        uart_puts(" bytes)\r\n");
    }
}
else if (strncmp(cmd, "cat ", 4) == 0) {
    // Đọc và in nội dung file
    const char *filename = cmd + 4;
    int fd = vfs_open(filename);
    if (fd < 0) { uart_puts("File not found\r\n"); }
    else {
        char buf[256]; int n;
        while ((n = vfs_read(fd, buf, sizeof(buf)-1)) > 0) {
            buf[n] = 0;
            uart_puts(buf);
        }
        vfs_close(fd);
    }
}
```

---

## Kết quả

```
ZKOS> ls
hello.txt    (48 bytes)
readme.txt   (63 bytes)
version      (7 bytes)
cpuinfo      (38 bytes)

ZKOS> cat hello.txt
Hello from ZKOS!
This file is served by RAMFS.

ZKOS> cat cpuinfo
Cortex-A55 @ i.MX93
Exception Level: EL2

ZKOS> write myfile "hello world"
Created and wrote to 'myfile'

ZKOS> ls
hello.txt    (48 bytes)
readme.txt   (63 bytes)
version      (7 bytes)
cpuinfo      (38 bytes)
myfile       (11 bytes)

ZKOS> cat myfile
hello world
```

---

## Toàn bộ stack của ZKOS

Đến bài 12, ZKOS có đầy đủ các tầng của một OS tối thiểu:

```
┌─────────────────────────────────────┐
│           Shell (main.c)            │  User interface
├─────────────────────────────────────┤
│  VFS: open/read/write/close/ls/cat  │  File abstraction
├─────────────────────────────────────┤
│  RAMFS: embedded + dynamic files    │  Storage backend
├─────────────────────────────────────┤
│  Syscalls: SVC #0 trap              │  User/kernel boundary
├─────────────────────────────────────┤
│  Scheduler: round-robin, preemptive │  Multitasking
├─────────────────────────────────────┤
│  Context Switch: save/restore regs  │  Task switching
├─────────────────────────────────────┤
│  Memory: MMU + page alloc + kmalloc │  Memory management
├─────────────────────────────────────┤
│  Interrupts: GICv3 + timer IRQ      │  Hardware events
├─────────────────────────────────────┤
│  Exceptions: vector table + crash   │  Error handling
├─────────────────────────────────────┤
│  UART + Timer + Watchdog drivers    │  Hardware drivers
├─────────────────────────────────────┤
│  stub.S: entry point, SP, VBAR      │  Boot
└─────────────────────────────────────┘
     FRDM-IMX93 / Cortex-A55 / EL2
```

---

## Bài học rút ra

1. **VFS = bảng hàm ảo:** `struct vfs_operations` là C tương đương của virtual dispatch trong C++ — cho phép thay filesystem backend mà không sửa code gọi.
2. **File descriptor = handle có trạng thái:** FD lưu ops, file index, và offset — mỗi `vfs_read()` tự động advance offset.
3. **RAMFS embedded files = zero-copy:** `data` pointer trỏ thẳng vào `.rodata`, không copy vào RAM riêng.
4. **kmalloc từ bài 8 giờ được dùng thật:** RAMFS writable files cấp phát 4KB từ heap. Các bài trước xây foundation, bài này dùng nó.
5. **FD 0/1/2 reserved:** Convention UNIX — stdin/stdout/stderr. Mọi process kế thừa 3 FD này khi được tạo.

## Tài liệu đã dùng

| Tài liệu | Phần | Nội dung |
|-----------|------|---------|
| OSTEP | Chapter 39-40 (Files and Directories) | VFS, inode, file descriptor |
| xv6 book | Chapter 8 (File system) | VFS design, file table |
| "The Unix Programming Environment" | Ch. 1 | "Everything is a file" philosophy |
| VinixOS `kernel/src/kernel/fs/vfs.c` | | VFS implementation pattern tham khảo |
| Linux kernel docs | VFS layer | struct file_operations (tương đương vfs_operations) |
