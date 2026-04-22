#include "uart.h"
#include "string.h"
#include "wdog.h"
#include "timer.h"
#include "gic.h"

/* ============================================================
 * IRQ Handler - Called from vectors.S
 * ============================================================ */
void irq_handler(void)
{
    uint32_t irq_id = gic_ack_irq();

    if (irq_id == TIMER_IRQ_ID) {
        timer_irq_handler();
    } else {
        uart_puts("[IRQ] Unknown IRQ: ");
        uart_putint(irq_id);
        uart_puts("\r\n");
    }

    gic_end_irq(irq_id);
}

/* ============================================================
 * Shell Command Handler
 * ============================================================ */
void exec_cmd(char* cmd)
{
    if(strcmp(cmd, "help") == 0)
    {
        uart_puts("Available commands:\r\n");
        uart_puts("  help   - Show this help message\r\n");
        uart_puts("  info   - System information\r\n");
        uart_puts("  hello  - Show greeting message\r\n");
        uart_puts("  uptime - Show system uptime\r\n");
        uart_puts("  ticks  - Show timer tick count\r\n");
        uart_puts("  crash  - Trigger a synchronous exception\r\n");
    }
    else if(strcmp(cmd, "info") == 0)
    {
        uart_puts("ZKOS v0.7 - Author: Nguyen Minh Tien\r\n");
    }
    else if(strcmp(cmd, "hello") == 0)
    {
        uart_puts("Welcome to ZKOS\r\n");
    }
    else if(strcmp(cmd, "uptime") == 0)
    {
        unsigned long uptime_s = get_timer_value() / get_timer_frequency();
        uart_puts("Uptime: ");
        uart_putint(uptime_s);
        uart_puts(" seconds\r\n");
    }
    else if(strcmp(cmd, "ticks") == 0)
    {
        uart_puts("Timer ticks: ");
        uart_putint(get_tick_count());
        uart_puts(" (10ms each)\r\n");
    }
    else if(strcmp(cmd, "crash") == 0)
    {
        uart_puts("Triggering data abort exception...\r\n");
        volatile int *null_ptr = (int *)0x0;
        int trigger = *null_ptr;
        (void)trigger;
    }
    else if(strlen(cmd) > 0)
    {
        uart_puts("Unknown cmd: ");
        uart_puts(cmd);
        uart_puts("\r\n");
    }
}

/* ============================================================
 * Main Entry Point
 * ============================================================ */
void main(void)
{
    wdog3_disable();

    uart_puts("\r\n=============================\r\n");
    uart_puts("  ZKOS v0.7 - Interrupts\r\n");
    uart_puts("=============================\r\n");

    /* Initialize GIC */
    gic_init();
    gic_enable_irq(TIMER_IRQ_ID);

    /* Initialize timer IRQ (10ms tick) */
    timer_init_irq(10);

    /* Unmask IRQs in DAIF */
    asm volatile("msr daifclr, #2");
    uart_puts("[BOOT] IRQ unmasked\r\n");

    uart_puts("Type 'help' for commands\r\n\r\n");
    uart_puts("ZKOS> ");

    char buf[64];
    int i = 0;

    while(1)
    {
        char c = uart_getc();
        if(c == '\r')
        {
            buf[i]='\0';
            uart_puts("\r\n");
            exec_cmd(buf);
            uart_puts("ZKOS> ");
            i = 0;
        }
        else if(c == '\b' | c == 0x7F)
        {
            if(i>0)
            {
                i --;
                uart_puts("\b \b");
            }
        }
        else
        {
            if(i < 63)
            {
                uart_putc(c);
                buf[i++] = c;
            }
        }
    }
}
