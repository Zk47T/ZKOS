.global vector_table_el2

.macro save_all_regs
    // Cấp phát 272 bytes trên Stack (bảo đảm chia hết cho 16)
    sub sp, sp, #272

    // Lưu các thanh ghi thông thường x0-x29
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

    // Lưu x30 (Link Register) tại vị trí byte 240
    str x30, [sp, #240]

    // Lưu ESR và ELR vào vị trí byte 248 và 256
    mrs x0, esr_el2
    mrs x1, elr_el2
    stp x0, x1, [sp, #248] 
.endm

// Define a macro for vectors we don't care about yet (Unhandled)
.macro invalid_vector
    b .
.endm

// ARMv8 Exception Vector Table
// Bắt buộc căn gốc (align) ở hệ số 11 tương đương 2048 bytes (0x800)
.align 11
vector_table_el2:

    // ==========================================
    // Current EL with SP0 (Không dùng mảng này)
    // ==========================================
    .align 7
    invalid_vector
    .align 7
    invalid_vector
    .align 7
    invalid_vector
    .align 7
    invalid_vector

    // ==========================================
    // Current EL with SPx (Zkos chạy ở đây)
    // ==========================================
    .align 7
    b handle_sync_el2
    .align 7
    invalid_vector    // IRQ (Chưa làm bài Ngắt)
    .align 7
    invalid_vector
    .align 7
    invalid_vector

    // ==========================================
    // Lower EL (AArch64) - Kernel bắt lỗi của App
    // ==========================================
    .align 7
    invalid_vector
    .align 7
    invalid_vector
    .align 7
    invalid_vector
    .align 7
    invalid_vector

    // ==========================================
    // Lower EL (AArch32)
    // ==========================================
    .align 7
    invalid_vector
    .align 7
    invalid_vector
    .align 7
    invalid_vector
    .align 7
    invalid_vector

handle_sync_el2:
    save_all_regs       // Khóa registers vào Stack
    mov x0, sp          // Truyền tham số con trỏ Context qua x0
    bl trap_handler     // Nhảy vào C Code !
    b .                 // Loop if return
