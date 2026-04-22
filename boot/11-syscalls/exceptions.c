#include "exception.h"
#include "uart.h"
#include "syscall.h"

/* ============================================================
 * Crash Handler (from Lesson 6)
 * ============================================================ */
static void crash_handler(struct exception_context *ctx)
{
    uart_puts("\r\n=======================================================\r\n");
    uart_puts("   [KERNEL PANIC] - SYNCHRONOUS EXCEPTION DETECTED!   \r\n");
    uart_puts("=======================================================\r\n");

    uart_puts("ESR_EL2 (Exception Syndrome): ");
    uart_puthex(ctx->esr);
    uart_puts("\r\n");

    uart_puts("ELR_EL2 (Faulting Address)  : ");
    uart_puthex(ctx->elr);
    uart_puts("\r\n");

    uart_puts("\r\n--- General Purpose Registers ---\r\n");
    for (int i = 0; i < 31; i++) {
        uart_puts("x");
        uart_putint(i);
        if (i < 10) uart_puts(" : ");
        else uart_puts(": ");
        uart_puthex(ctx->x[i]);
        uart_puts("\r\n");
    }

    uart_puts("\r\nSystem halted.\r\n");
    while (1) {}
}

/* ============================================================
 * Synchronous Dispatch (NEW in Lesson 11)
 * Checks ESR: SVC → syscall, otherwise → crash
 * ============================================================ */
void sync_dispatch(struct exception_context *ctx)
{
    uint32_t esr = (uint32_t)ctx->esr;
    uint32_t ec = (esr >> 26) & 0x3F;

    if (ec == 0x15) {
        /* EC 0x15 = SVC instruction from AArch64 */
        syscall_dispatch(ctx);
    } else {
        crash_handler(ctx);
    }
}
