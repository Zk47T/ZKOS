#ifndef TASK_H
#define TASK_H

#include "types.h"

/* ============================================================
 * Task Constants
 * ============================================================ */
#define MAX_TASKS       8
#define TASK_STACK_SIZE  8192  // 8KB per task

/* Task states */
#define TASK_READY      0
#define TASK_RUNNING    1
#define TASK_DEAD       2

/* ============================================================
 * Task Context - saved/restored during context switch
 * ============================================================
 *
 * AArch64 calling convention:
 *   x0-x18:  Caller-saved (scratch) - NOT saved during coop switch
 *   x19-x28: Callee-saved - MUST be preserved
 *   x29 (FP): Frame pointer - callee-saved
 *   x30 (LR): Link register - return address
 *   SP:       Stack pointer
 *
 * For cooperative switch, we only save callee-saved + SP + LR
 */
struct task_context {
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29;   // FP
    uint64_t x30;   // LR (return address)
    uint64_t sp;     // Stack pointer
};

/* ============================================================
 * Task Control Block
 * ============================================================ */
struct task_struct {
    struct task_context ctx;        // Saved CPU context
    uint8_t stack[TASK_STACK_SIZE]; // Task's own stack
    uint32_t pid;
    uint32_t state;
    const char *name;
};

/* ============================================================
 * Task API
 * ============================================================ */
void task_init(void);
int  task_create(const char *name, void (*entry)(void));
void yield(void);

struct task_struct *task_get_current(void);
struct task_struct *task_get(int pid);
int task_get_count(void);

/* Assembly function */
extern void context_switch(struct task_context *old_ctx, struct task_context *new_ctx);

#endif /* TASK_H */
