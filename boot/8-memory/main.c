#include "uart.h"
#include "string.h"
#include "wdog.h"
#include "timer.h"
#include "gic.h"
#include "mm.h"

/* ============================================================
 * IRQ Handler
 * ============================================================ */
void irq_handler(void)
{
    uint32_t irq_id = gic_ack_irq();
    if (irq_id == TIMER_IRQ_ID) {
        timer_irq_handler();
    }
    gic_end_irq(irq_id);
}

/* ============================================================
 * Shell
 * ============================================================ */
void exec_cmd(char* cmd)
{
    if(strcmp(cmd, "help") == 0) {
        uart_puts("Available commands:\r\n");
        uart_puts("  help    - Show this help message\r\n");
        uart_puts("  info    - System information\r\n");
        uart_puts("  uptime  - Show system uptime\r\n");
        uart_puts("  ticks   - Show timer tick count\r\n");
        uart_puts("  meminfo - Memory usage statistics\r\n");
        uart_puts("  crash   - Trigger exception\r\n");
    }
    else if(strcmp(cmd, "info") == 0) {
        uart_puts("ZKOS v0.8 - Author: Nguyen Minh Tien\r\n");
    }
    else if(strcmp(cmd, "uptime") == 0) {
        unsigned long uptime_s = get_timer_value() / get_timer_frequency();
        uart_puts("Uptime: ");
        uart_putint(uptime_s);
        uart_puts(" seconds\r\n");
    }
    else if(strcmp(cmd, "ticks") == 0) {
        uart_puts("Timer ticks: ");
        uart_putint(get_tick_count());
        uart_puts("\r\n");
    }
    else if(strcmp(cmd, "meminfo") == 0) {
        uart_puts("--- Memory Info ---\r\n");
        uart_puts("Pages: ");
        uart_putint(page_get_used_count());
        uart_puts(" used / ");
        uart_putint(page_get_used_count() + page_get_free_count());
        uart_puts(" total (");
        uart_putint(page_get_free_count() * 4);
        uart_puts(" KB free)\r\n");
        uart_puts("Heap:  ");
        uart_putint(heap_get_used());
        uart_puts(" bytes allocated\r\n");
    }
    else if(strcmp(cmd, "crash") == 0) {
        uart_puts("Triggering data abort...\r\n");
        volatile int *p = (int *)0x0;
        (void)*p;
    }
    else if(strlen(cmd) > 0) {
        uart_puts("Unknown cmd: ");
        uart_puts(cmd);
        uart_puts("\r\n");
    }
}

void main(void)
{
    wdog3_disable();

    uart_puts("\r\n=============================\r\n");
    uart_puts("  ZKOS v0.8 - Memory Mgmt\r\n");
    uart_puts("=============================\r\n");

    /* Memory management init */
    mmu_init();
    page_alloc_init();
    heap_init();

    /* Interrupts */
    gic_init();
    gic_enable_irq(TIMER_IRQ_ID);
    timer_init_irq(10);
    asm volatile("msr daifclr, #2");

    /* Test allocations */
    uart_puts("[TEST] Allocating 3 pages...\r\n");
    void *p1 = page_alloc();
    void *p2 = page_alloc();
    void *p3 = page_alloc();
    uart_puts("  Page 1: "); uart_puthex((unsigned long)p1); uart_puts("\r\n");
    uart_puts("  Page 2: "); uart_puthex((unsigned long)p2); uart_puts("\r\n");
    uart_puts("  Page 3: "); uart_puthex((unsigned long)p3); uart_puts("\r\n");
    page_free(p2);
    uart_puts("  Freed page 2\r\n");

    uart_puts("Type 'help' for commands\r\n\r\n");
    uart_puts("ZKOS> ");

    char buf[64];
    int i = 0;
    while(1) {
        char c = uart_getc();
        if(c == '\r') {
            buf[i]='\0';
            uart_puts("\r\n");
            exec_cmd(buf);
            uart_puts("ZKOS> ");
            i = 0;
        } else if(c == '\b' || c == 0x7F) {
            if(i>0) { i--; uart_puts("\b \b"); }
        } else {
            if(i < 63) { uart_putc(c); buf[i++] = c; }
        }
    }
}
