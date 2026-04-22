#include "task.h"
#include "uart.h"
#include "string.h"

/* ============================================================
 * Task Table (shared with scheduler)
 * ============================================================ */
static struct task_struct tasks[MAX_TASKS];
static int current_task = -1;
static int task_count = 0;

void task_init(void)
{
    memset(tasks, 0, sizeof(tasks));
    task_count = 0;
    current_task = -1;
}

int task_create(const char *name, void (*entry)(void))
{
    if (task_count >= MAX_TASKS) return -1;

    struct task_struct *t = &tasks[task_count];
    memset(t, 0, sizeof(*t));

    t->pid = task_count;
    t->name = name;
    t->state = TASK_READY;

    uint64_t stack_top = (uint64_t)t->stack + TASK_STACK_SIZE;
    stack_top &= ~0xFUL;

    t->ctx.sp = stack_top;
    t->ctx.x30 = (uint64_t)entry;
    t->ctx.x29 = 0;

    uart_puts("[TASK] Created: ");
    uart_puts(name);
    uart_puts(" (PID ");
    uart_putint(task_count);
    uart_puts(")\r\n");

    task_count++;
    return t->pid;
}

void yield(void)
{
    if (task_count <= 1) return;
    int prev = current_task;
    int next = prev;

    for (int i = 0; i < task_count; i++) {
        next = (next + 1) % task_count;
        if (tasks[next].state != TASK_DEAD) break;
    }
    if (next == prev) return;

    if (tasks[prev].state == TASK_RUNNING) tasks[prev].state = TASK_READY;
    tasks[next].state = TASK_RUNNING;
    current_task = next;

    context_switch(&tasks[prev].ctx, &tasks[next].ctx);
}

struct task_struct *task_get_current(void) {
    if (current_task < 0) return NULL;
    return &tasks[current_task];
}
struct task_struct *task_get(int pid) {
    if (pid < 0 || pid >= task_count) return NULL;
    return &tasks[pid];
}
int task_get_count(void) { return task_count; }
