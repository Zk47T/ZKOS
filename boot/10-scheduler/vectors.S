.global vector_table_el2

.macro save_all_regs
    sub sp, sp, #272
    stp x0, x1, [sp, #16 * 0]
    stp x2, x3, [sp, #16 * 1]
    stp x4, x5, [sp, #16 * 2]
    stp x6, x7, [sp, #16 * 3]
    stp x8, x9, [sp, #16 * 4]
    stp x10, x11, [sp, #16 * 5]
    stp x12, x13, [sp, #16 * 6]
    stp x14, x15, [sp, #16 * 7]
    stp x16, x17, [sp, #16 * 8]
    stp x18, x19, [sp, #16 * 9]
    stp x20, x21, [sp, #16 * 10]
    stp x22, x23, [sp, #16 * 11]
    stp x24, x25, [sp, #16 * 12]
    stp x26, x27, [sp, #16 * 13]
    stp x28, x29, [sp, #16 * 14]
    str x30, [sp, #240]
    mrs x0, esr_el2
    mrs x1, elr_el2
    stp x0, x1, [sp, #248]
.endm

.macro restore_all_regs
    ldp x0, x1, [sp, #16 * 0]
    ldp x2, x3, [sp, #16 * 1]
    ldp x4, x5, [sp, #16 * 2]
    ldp x6, x7, [sp, #16 * 3]
    ldp x8, x9, [sp, #16 * 4]
    ldp x10, x11, [sp, #16 * 5]
    ldp x12, x13, [sp, #16 * 6]
    ldp x14, x15, [sp, #16 * 7]
    ldp x16, x17, [sp, #16 * 8]
    ldp x18, x19, [sp, #16 * 9]
    ldp x20, x21, [sp, #16 * 10]
    ldp x22, x23, [sp, #16 * 11]
    ldp x24, x25, [sp, #16 * 12]
    ldp x26, x27, [sp, #16 * 13]
    ldp x28, x29, [sp, #16 * 14]
    ldr x30, [sp, #240]
    add sp, sp, #272
.endm

.macro invalid_vector
    b .
.endm

.align 11
vector_table_el2:

    // Current EL with SP0
    .align 7
    invalid_vector
    .align 7
    invalid_vector
    .align 7
    invalid_vector
    .align 7
    invalid_vector

    // Current EL with SPx (ZKOS runs here)
    .align 7
    b handle_sync_el2
    .align 7
    b handle_irq_el2
    .align 7
    invalid_vector
    .align 7
    invalid_vector

    // Lower EL (AArch64)
    .align 7
    invalid_vector
    .align 7
    invalid_vector
    .align 7
    invalid_vector
    .align 7
    invalid_vector

    // Lower EL (AArch32)
    .align 7
    invalid_vector
    .align 7
    invalid_vector
    .align 7
    invalid_vector
    .align 7
    invalid_vector

handle_sync_el2:
    save_all_regs
    mov x0, sp
    bl trap_handler
    b .

handle_irq_el2:
    save_all_regs
    bl irq_handler
    restore_all_regs
    eret
