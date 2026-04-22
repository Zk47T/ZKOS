#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"
#include "exception.h"

/* ============================================================
 * Syscall Numbers
 * ============================================================ */
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_YIELD   3
#define SYS_GETPID  4
#define SYS_EXIT    5

/* ============================================================
 * Syscall Dispatcher (called from exception handler)
 * ============================================================ */
void syscall_dispatch(struct exception_context *ctx);

/* ============================================================
 * User-space wrappers (run in EL2 alongside kernel for now)
 * ============================================================ */
long sys_write(int fd, const char *buf, size_t count);
long sys_yield(void);
long sys_getpid(void);
void sys_exit(void);

#endif /* SYSCALL_H */
