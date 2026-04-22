#include "uart.h"
#include "string.h"
#include "wdog.h"
#include "timer.h"
#include "gic.h"
#include "mm.h"
#include "task.h"

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
 * Demo Tasks - print characters cooperatively
 * ============================================================ */
void task_A(void)
{
    while (1) {
        uart_puts("A");
        sleep_ms(500);
        yield();
    }
}

void task_B(void)
{
    while (1) {
        uart_puts("B");
        sleep_ms(500);
        yield();
    }
}

/* ============================================================
 * Shell Task
 * ============================================================ */
static void exec_cmd(char *cmd);

void shell_task(void)
{
    uart_puts("ZKOS> ");
    char buf[64];
    int i = 0;

    while (1) {
        /* Check if there is a character available (non-blocking would be ideal,
           but for simplicity we still block here and yield periodically) */
        char c = uart_getc();
        if (c == '\r') {
            buf[i] = '\0';
            uart_puts("\r\n");
            exec_cmd(buf);
            uart_puts("ZKOS> ");
            i = 0;
        } else if (c == '\b' || c == 0x7F) {
            if (i > 0) { i--; uart_puts("\b \b"); }
        } else {
            if (i < 63) { uart_putc(c); buf[i++] = c; }
        }
    }
}

static void exec_cmd(char *cmd)
{
    if (strcmp(cmd, "help") == 0) {
        uart_puts("Available commands:\r\n");
        uart_puts("  help    - Show this help\r\n");
        uart_puts("  info    - System information\r\n");
        uart_puts("  uptime  - System uptime\r\n");
        uart_puts("  ticks   - Timer ticks\r\n");
        uart_puts("  meminfo - Memory stats\r\n");
        uart_puts("  ps      - List tasks\r\n");
        uart_puts("  crash   - Trigger exception\r\n");
    }
    else if (strcmp(cmd, "info") == 0) {
        uart_puts("ZKOS v0.9 - Author: Nguyen Minh Tien\r\n");
    }
    else if (strcmp(cmd, "uptime") == 0) {
        unsigned long s = get_timer_value() / get_timer_frequency();
        uart_puts("Uptime: "); uart_putint(s); uart_puts(" seconds\r\n");
    }
    else if (strcmp(cmd, "ticks") == 0) {
        uart_puts("Timer ticks: "); uart_putint(get_tick_count()); uart_puts("\r\n");
    }
    else if (strcmp(cmd, "meminfo") == 0) {
        uart_puts("Pages: ");
        uart_putint(page_get_used_count()); uart_puts(" used / ");
        uart_putint(page_get_used_count() + page_get_free_count());
        uart_puts(" total\r\nHeap: ");
        uart_putint(heap_get_used()); uart_puts(" bytes\r\n");
    }
    else if (strcmp(cmd, "ps") == 0) {
        uart_puts("PID  STATE    NAME\r\n");
        for (int i = 0; i < task_get_count(); i++) {
            struct task_struct *t = task_get(i);
            if (!t) continue;
            uart_puts("  "); uart_putint(t->pid);
            uart_puts("  ");
            if (t->state == TASK_RUNNING) uart_puts("RUNNING  ");
            else if (t->state == TASK_READY) uart_puts("READY    ");
            else uart_puts("DEAD     ");
            uart_puts(t->name);
            uart_puts("\r\n");
        }
    }
    else if (strcmp(cmd, "crash") == 0) {
        volatile int *p = (int *)0; (void)*p;
    }
    else if (strlen(cmd) > 0) {
        uart_puts("Unknown: "); uart_puts(cmd); uart_puts("\r\n");
    }
}

/* ============================================================
 * Main
 * ============================================================ */
void main(void)
{
    wdog3_disable();

    uart_puts("\r\n================================\r\n");
    uart_puts("  ZKOS v0.9 - Context Switch\r\n");
    uart_puts("================================\r\n");

    mmu_init();
    page_alloc_init();
    heap_init();

    gic_init();
    gic_enable_irq(TIMER_IRQ_ID);
    timer_init_irq(10);
    asm volatile("msr daifclr, #2");

    /* Create tasks */
    task_init();
    task_create("shell", shell_task);
    task_create("task_A", task_A);
    task_create("task_B", task_B);

    uart_puts("\r\nStarting cooperative multitasking...\r\n");
    uart_puts("Tasks A and B print while shell waits for input\r\n\r\n");

    /* Start the first task (shell) - does not return */
    task_start();
}
