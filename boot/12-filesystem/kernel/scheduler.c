#include "scheduler.h"
#include "uart.h"
#include "string.h"

/* ============================================================
 * Scheduler State
 * ============================================================ */
static struct task_struct *run_queue[MAX_TASKS];
static int rq_count = 0;
static int current_idx = 0;
static int tick_counter = 0;
static int started = 0;

#define TIME_SLICE  10  // 10 ticks = 100ms at 10ms/tick

void scheduler_init(void)
{
    memset(run_queue, 0, sizeof(run_queue));
    rq_count = 0;
    current_idx = 0;
    tick_counter = 0;
    started = 0;
    uart_puts("[SCHED] Scheduler initialized\r\n");
}

void scheduler_add(struct task_struct *task)
{
    if (rq_count >= MAX_TASKS) return;
    run_queue[rq_count++] = task;
    uart_puts("[SCHED] Added task: ");
    uart_puts(task->name);
    uart_puts("\r\n");
}

/* ============================================================
 * schedule - Pick next READY task and switch
 * ============================================================ */
void schedule(void)
{
    if (rq_count <= 1) return;

    int prev_idx = current_idx;
    int next_idx = current_idx;

    /* Round-robin: find next READY task */
    for (int i = 0; i < rq_count; i++) {
        next_idx = (next_idx + 1) % rq_count;
        if (run_queue[next_idx]->state != TASK_DEAD) {
            break;
        }
    }

    if (next_idx == prev_idx) return;

    struct task_struct *prev = run_queue[prev_idx];
    struct task_struct *next = run_queue[next_idx];

    if (prev->state == TASK_RUNNING)
        prev->state = TASK_READY;
    next->state = TASK_RUNNING;
    current_idx = next_idx;

    context_switch(&prev->ctx, &next->ctx);
}

/* ============================================================
 * scheduler_tick - Called from timer IRQ handler
 * ============================================================
 * Counts ticks and triggers reschedule when time slice expires
 */
void scheduler_tick(void)
{
    if (!started) return;

    tick_counter++;
    if (tick_counter >= TIME_SLICE) {
        tick_counter = 0;
        /* We set a flag and let the IRQ return path call schedule.
         * But for simplicity, we call schedule directly from IRQ context.
         * This works because each task has its own stack. */
        schedule();
    }
}

void scheduler_start(void)
{
    if (rq_count == 0) return;

    started = 1;
    current_idx = 0;
    run_queue[0]->state = TASK_RUNNING;

    uart_puts("[SCHED] Starting preemptive scheduler\r\n");

    /* Jump to first task */
    struct task_context dummy;
    context_switch(&dummy, &run_queue[0]->ctx);
}
