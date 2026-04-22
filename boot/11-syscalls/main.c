#include "uart.h"
#include "string.h"
#include "wdog.h"
#include "timer.h"
#include "gic.h"
#include "mm.h"
#include "task.h"
#include "scheduler.h"
#include "syscall.h"

void irq_handler(void)
{
    uint32_t irq_id = gic_ack_irq();
    if (irq_id == TIMER_IRQ_ID) {
        timer_irq_handler();
        scheduler_tick();
    }
    gic_end_irq(irq_id);
}

/* ============================================================
 * Demo User Task (Uses Syscalls)
 * ============================================================ */
void user_task(void)
{
    const char *msg = "Hello from User Task (via sys_write)!\r\n";
    long pid = sys_getpid();

    while (1) {
        sys_write(1, "[PID ", 5);
        
        char pid_str[2];
        pid_str[0] = '0' + pid;
        pid_str[1] = '\0';
        sys_write(1, pid_str, 1);
        sys_write(1, "] ", 2);
        
        sys_write(1, msg, strlen(msg));
        
        /* Instead of pure delay, yield voluntarily to other tasks */
        sys_yield();
        sleep_ms(1000);
    }
}

/* ============================================================
 * Shell Task
 * ============================================================ */
static void exec_cmd(char *cmd);

void shell_task(void)
{
    sys_write(1, "ZKOS> ", 6); // Using syscall!
    char buf[64];
    int i = 0;

    while (1) {
        char c = uart_getc();
        if (c == '\r') {
            buf[i] = '\0';
            sys_write(1, "\r\n", 2);
            exec_cmd(buf);
            sys_write(1, "ZKOS> ", 6);
            i = 0;
        } else if (c == '\b' || c == 0x7F) {
            if (i > 0) { i--; sys_write(1, "\b \b", 3); }
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
        uart_puts("  meminfo - Memory stats\r\n");
        uart_puts("  ps      - List tasks\r\n");
        uart_puts("  spawn   - Create syscall test task\r\n");
        uart_puts("  syscall - Test direct sys_write\r\n");
    }
    else if (strcmp(cmd, "info") == 0) {
        uart_puts("ZKOS v0.11 - Syscalls (SVC)\r\n");
    }
    else if (strcmp(cmd, "meminfo") == 0) {
        uart_puts("Pages: ");
        uart_putint(page_get_used_count()); uart_puts("/");
        uart_putint(page_get_used_count() + page_get_free_count());
        uart_puts("  Heap: "); uart_putint(heap_get_used()); uart_puts("\r\n");
    }
    else if (strcmp(cmd, "ps") == 0) {
        uart_puts("PID  STATE    NAME\r\n");
        for (int i = 0; i < task_get_count(); i++) {
            struct task_struct *t = task_get(i);
            if (!t) continue;
            uart_puts("  "); uart_putint(t->pid); uart_puts("  ");
            if (t->state == TASK_RUNNING) uart_puts("RUNNING  ");
            else if (t->state == TASK_READY) uart_puts("READY    ");
            else uart_puts("DEAD     ");
            uart_puts(t->name); uart_puts("\r\n");
        }
    }
    else if (strcmp(cmd, "spawn") == 0) {
        int pid = task_create("usertask", user_task);
        scheduler_add(task_get(pid));
        uart_puts("Spawned usertask PID "); uart_putint(pid); uart_puts("\r\n");
    }
    else if (strcmp(cmd, "syscall") == 0) {
        const char *m = "This was printed via sys_write() !!\r\n";
        sys_write(1, m, strlen(m));
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

    uart_puts("\r\n=============================\r\n");
    uart_puts("  ZKOS v0.11 - Syscalls (SVC)\r\n");
    uart_puts("=============================\r\n");

    mmu_init();
    page_alloc_init();
    heap_init();

    gic_init();
    gic_enable_irq(TIMER_IRQ_ID);
    timer_init_irq(10);

    task_init();
    scheduler_init();

    int shell_pid = task_create("shell", shell_task);
    scheduler_add(task_get(shell_pid));

    asm volatile("msr daifclr, #2");
    scheduler_start(); 
}
