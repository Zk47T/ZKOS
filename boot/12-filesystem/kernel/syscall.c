#include "syscall.h"
#include "uart.h"
#include "task.h"
#include "scheduler.h"

/* ============================================================
 * Kernel Syscall Handlers
 * ============================================================ */

static long do_write(int fd, const char *buf, size_t count)
{
    /* Simple: ignore FD and output to UART directly */
    for (size_t i = 0; i < count; i++) {
        uart_putc(buf[i]);
    }
    return count;
}

static long do_yield(void)
{
    /* Mark flag, actual yield happens on return */
    schedule();
    return 0;
}

static long do_getpid(void)
{
    struct task_struct *current = task_get_current();
    if (current) return current->pid;
    return -1;
}

static void do_exit(void)
{
    struct task_struct *current = task_get_current();
    if (current) {
        current->state = TASK_DEAD;
        uart_puts("[SYS] Task ");
        uart_putint(current->pid);
        uart_puts(" exited.\r\n");
        schedule(); // Switch away forever
    }
}

/* ============================================================
 * Syscall Dispatcher
 * ============================================================
 * x8 = syscall number
 * x0-x5 = arguments
 * Return value placed in ctx->x[0]
 */
void syscall_dispatch(struct exception_context *ctx)
{
    uint64_t syscall_num = ctx->x[8];
    uint64_t ret = -1;

    switch (syscall_num) {
        case SYS_WRITE:
            ret = do_write((int)ctx->x[0], (const char *)ctx->x[1], (size_t)ctx->x[2]);
            break;
        case SYS_READ:
            /* Not implemented for simple demo */
            ret = -1;
            break;
        case SYS_YIELD:
            ret = do_yield();
            break;
        case SYS_GETPID:
            ret = do_getpid();
            break;
        case SYS_EXIT:
            do_exit();
            ret = 0; // Should not return here normally if exit
            break;
        default:
            uart_puts("[SYS] Unknown syscall: ");
            uart_putint(syscall_num);
            uart_puts("\r\n");
            break;
    }

    ctx->x[0] = ret;  // Set return value in saved x0
}

/* ============================================================
 * User-space Wrappers (using SVC #0)
 * ============================================================ */
long sys_write(int fd, const char *buf, size_t count)
{
    long ret;
    asm volatile(
        "mov x8, %1\n"
        "mov x0, %2\n"
        "mov x1, %3\n"
        "mov x2, %4\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "I"(SYS_WRITE), "r"((long)fd), "r"(buf), "r"(count)
        : "x0", "x1", "x2", "x8", "memory"
    );
    return ret;
}

long sys_yield(void)
{
    long ret;
    asm volatile(
        "mov x8, %1\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "I"(SYS_YIELD)
        : "x0", "x8", "memory"
    );
    return ret;
}

long sys_getpid(void)
{
    long ret;
    asm volatile(
        "mov x8, %1\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "I"(SYS_GETPID)
        : "x0", "x8", "memory"
    );
    return ret;
}

void sys_exit(void)
{
    asm volatile(
        "mov x8, %0\n"
        "svc #0\n"
        :: "I"(SYS_EXIT)
        : "x8", "memory"
    );
}
