#include "task.h"
#include "uart.h"
#include "string.h"

/* ============================================================
 * Task Table
 * ============================================================ */
static struct task_struct tasks[MAX_TASKS];
static int current_task = -1;
static int task_count = 0;

void task_init(void)
{
    memset(tasks, 0, sizeof(tasks));
    task_count = 0;
    current_task = -1;
    uart_puts("[TASK] Task subsystem initialized\r\n");
}

/* ============================================================
 * task_create - Create a new task
 * ============================================================
 *
 * Sets up the initial context so that when context_switch()
 * restores this task for the first time, it will:
 *   - Set SP to top of task's stack
 *   - Set LR (x30) to entry function
 *   - ret → jumps to entry function
 */
int task_create(const char *name, void (*entry)(void))
{
    if (task_count >= MAX_TASKS) {
        uart_puts("[TASK] Error: task table full\r\n");
        return -1;
    }

    struct task_struct *t = &tasks[task_count];
    memset(t, 0, sizeof(*t));

    t->pid = task_count;
    t->name = name;
    t->state = TASK_READY;

    /* Stack grows downward: SP starts at top of stack array */
    uint64_t stack_top = (uint64_t)t->stack + TASK_STACK_SIZE;
    /* Ensure 16-byte alignment */
    stack_top &= ~0xFUL;

    t->ctx.sp = stack_top;
    t->ctx.x30 = (uint64_t)entry;  // LR = entry point
    t->ctx.x29 = 0;                // FP = 0 (base frame)

    uart_puts("[TASK] Created task ");
    uart_putint(t->pid);
    uart_puts(": '");
    uart_puts(name);
    uart_puts("'\r\n");

    task_count++;
    return t->pid;
}

/* ============================================================
 * yield - Cooperative context switch
 * ============================================================ */
void yield(void)
{
    if (task_count <= 1) return;

    int prev = current_task;
    int next = prev;

    /* Find next READY task (round-robin) */
    for (int i = 0; i < task_count; i++) {
        next = (next + 1) % task_count;
        if (tasks[next].state == TASK_READY || tasks[next].state == TASK_RUNNING) {
            break;
        }
    }

    if (next == prev) return;  // No other task to switch to

    /* Update states */
    if (tasks[prev].state == TASK_RUNNING)
        tasks[prev].state = TASK_READY;
    tasks[next].state = TASK_RUNNING;
    current_task = next;

    /* Perform the actual context switch */
    context_switch(&tasks[prev].ctx, &tasks[next].ctx);
}

/* ============================================================
 * Getters
 * ============================================================ */
struct task_struct *task_get_current(void)
{
    if (current_task < 0) return NULL;
    return &tasks[current_task];
}

struct task_struct *task_get(int pid)
{
    if (pid < 0 || pid >= task_count) return NULL;
    return &tasks[pid];
}

int task_get_count(void) { return task_count; }

/* ============================================================
 * Start first task (called once from main)
 * ============================================================
 * Instead of context_switch, we manually load the first task
 */
void task_start(void)
{
    if (task_count == 0) return;

    current_task = 0;
    tasks[0].state = TASK_RUNNING;

    /* Load first task's context and jump to it */
    struct task_context *ctx = &tasks[0].ctx;

    asm volatile(
        "mov sp, %0\n"
        "br %1\n"
        :: "r"(ctx->sp), "r"(ctx->x30)
    );
}
