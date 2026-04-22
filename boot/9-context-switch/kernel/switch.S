/* ============================================================
 * context_switch.S - AArch64 Cooperative Context Switch
 * ============================================================
 *
 * void context_switch(struct task_context *old, struct task_context *new)
 *   x0 = pointer to old task's context (save here)
 *   x1 = pointer to new task's context (restore from here)
 *
 * struct task_context layout (offsets):
 *   0x00: x19
 *   0x08: x20
 *   0x10: x21
 *   0x18: x22
 *   0x20: x23
 *   0x28: x24
 *   0x30: x25
 *   0x38: x26
 *   0x40: x27
 *   0x48: x28
 *   0x50: x29 (FP)
 *   0x58: x30 (LR)
 *   0x60: SP
 */

.global context_switch
.section .text

context_switch:
    /* Save callee-saved registers of OLD task */
    stp x19, x20, [x0, #0x00]
    stp x21, x22, [x0, #0x10]
    stp x23, x24, [x0, #0x20]
    stp x25, x26, [x0, #0x30]
    stp x27, x28, [x0, #0x40]
    stp x29, x30, [x0, #0x50]
    mov x9, sp
    str x9, [x0, #0x60]

    /* Restore callee-saved registers of NEW task */
    ldp x19, x20, [x1, #0x00]
    ldp x21, x22, [x1, #0x10]
    ldp x23, x24, [x1, #0x20]
    ldp x25, x26, [x1, #0x30]
    ldp x27, x28, [x1, #0x40]
    ldp x29, x30, [x1, #0x50]
    ldr x9, [x1, #0x60]
    mov sp, x9

    /* Return to new task's saved LR (x30) */
    ret
