#include "exception.h"
#include "uart.h"

void trap_handler(struct exception_context *ctx)
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
    
    uart_puts("\r\nSystem halted. Please reset the board.\r\n");

    // Spin infinitely. Watchdog is disabled, so we wait for manual reset
    while (1) {
        // Halt
    }
}
