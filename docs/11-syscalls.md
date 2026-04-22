# Bài 11 — Syscalls: Ranh giới User / Kernel

Bài trước, shell và task đều chạy ở EL2 — chúng có thể ghi thẳng vào UART, gọi `context_switch()` trực tiếp. Trong OS thật, **user program không được phép đụng vào hardware hay gọi kernel function trực tiếp**. Bài này xây dựng *cánh cửa kiểm soát* giữa user và kernel: **system call** qua lệnh `SVC`.

---

## Bối cảnh: Tại sao cần syscall?

Hãy tưởng tượng không có ranh giới user/kernel: mọi chương trình đều có thể ghi `*(uint32_t*)0x44380000 = 'A'` để trực tiếp gửi ký tự qua UART. Khi nhiều chương trình chạy đồng thời, output lẫn lộn hỗn loạn. Hoặc một chương trình malware ghi thẳng lên bảng page table của chương trình khác.

**Syscall** là *trap có kiểm soát*: user code muốn làm gì với hardware/OS, phải "gõ cửa" qua một interface chuẩn. Kernel quyết định có chấp nhận yêu cầu không, và thực hiện thay mặt user.

```
User Task (EL0 trong OS thật)     Kernel (EL1/EL2)
─────────────────────────────     ─────────────────
sys_write(1, "hello", 5)          
  → mov x8, SYS_WRITE             
  → svc #0         ─── trap ───→  syscall_dispatch(ctx)
                                    → do_write(fd, buf, len)
                                    → uart_putc(...) × 5
                   ←── eret ────    ctx->x[0] = 5 (return value)
  ← return value in x0            
```

---

## Luồng thực thi: SVC đến dispatch

**Bước 1: User code gọi wrapper**

[boot/11-syscalls/kernel/syscall.c](../boot/11-syscalls/kernel/syscall.c) định nghĩa user-space wrappers dùng inline assembly:

```c
long sys_write(int fd, const char *buf, size_t count)
{
    long ret;
    asm volatile(
        "mov x8, %1\n"   // x8 = syscall number (convention Linux AArch64)
        "mov x0, %2\n"   // x0 = arg1: fd
        "mov x1, %3\n"   // x1 = arg2: buf
        "mov x2, %4\n"   // x2 = arg3: count
        "svc #0\n"        // Trap vào kernel
        "mov %0, x0\n"   // x0 = return value (sau khi kernel điền vào)
        : "=r"(ret)
        : "I"(SYS_WRITE), "r"((long)fd), "r"(buf), "r"(count)
        : "x0", "x1", "x2", "x8", "memory"
    );
    return ret;
}
```

**Tại sao x8 cho syscall number thay vì x0?**  
x0-x7 là argument registers. Nếu dùng x0 cho số syscall thì arg1 bị đẩy xuống x1 — mất 1 argument slot. Linux AArch64 dùng x8 làm syscall number, giữ x0-x5 cho 6 arguments. ZKOS follow convention này.

**Bước 2: `svc #0` kích hoạt synchronous exception**

CPU nhảy vào vector table entry `handle_sync` (Current EL with SPx). ESR_EL2 chứa:
- `EC = 0x15` → exception class = "SVC instruction execution"  
- `ISS` = số ngay sau `svc` (ở đây là 0)

**Bước 3: exceptions.c phân tích ESR và gọi dispatch**

[boot/11-syscalls/exceptions.c](../boot/11-syscalls/exceptions.c) — modified từ bài 6:

```c
void trap_handler(struct exception_context *ctx)
{
    uint64_t ec = (ctx->esr >> 26) & 0x3F;  // Exception Class [31:26]

    if (ec == 0x15) {
        // SVC instruction → syscall
        syscall_dispatch(ctx);
        // ELR đã trỏ đến lệnh TIẾP THEO sau svc #0
        // → eret sẽ return đúng vào user code
    } else {
        // Data abort, undefined instruction...
        uart_puts("[TRAP] Crash! ESR=");
        uart_puthex(ctx->esr);
        // ... dump registers ...
        while (1) {}
    }
}
```

**Bước 4: syscall_dispatch() — kernel handler**

```c
// boot/11-syscalls/kernel/syscall.c
void syscall_dispatch(struct exception_context *ctx)
{
    uint64_t syscall_num = ctx->x[8];  // x8 được save trong exception frame
    uint64_t ret = -1;

    switch (syscall_num) {
        case SYS_WRITE:   // 1
            ret = do_write((int)ctx->x[0], (const char *)ctx->x[1], (size_t)ctx->x[2]);
            break;
        case SYS_YIELD:   // 3
            ret = do_yield();  // gọi schedule()
            break;
        case SYS_GETPID:  // 4
            ret = do_getpid(); // trả về current task's PID
            break;
        case SYS_EXIT:    // 5
            do_exit();         // mark task DEAD, schedule()
            break;
        default:
            uart_puts("[SYS] Unknown syscall\r\n");
            break;
    }

    ctx->x[0] = ret;  // Ghi return value vào saved x0 → khi eret, user thấy x0=ret
}
```

**Điểm then chốt — `ctx->x[0] = ret`:**  
`ctx` là pointer vào stack frame của IRQ. Khi `eret` thực thi, CPU restore x0 từ frame đó. Bằng cách ghi vào `ctx->x[0]`, kernel "chèn" return value vào x0 của user code — user code thấy return value trong x0 như thể hàm bình thường return.

---

## Vectors.S — SVC phải `eret`, không thể `b .`

Bài 6, sau khi xử lý synchronous exception (crash), ta gọi `while(1)` vì không có chỗ để return. SVC thì khác — phải return về user code. [boot/11-syscalls/vectors.S](../boot/11-syscalls/vectors.S) modified:

```asm
handle_sync_el2:
    save_all_regs           // Lưu 31 regs + ESR + ELR (272 bytes)
    mov x0, sp
    bl trap_handler         // Xử lý (syscall hoặc crash)

    // Restore và return — KHÁC với bài 6!
    ldp x0, x1,   [sp, #16*0]
    ldp x2, x3,   [sp, #16*1]
    // ... restore tất cả ...
    ldp x29, x30, [sp, #16*15]
    add sp, sp, #272

    eret  // Return về ELR (lệnh sau svc #0), với SPSR restore DAIF, EL...
```

---

## Syscall Numbers

[boot/11-syscalls/include/syscall.h](../boot/11-syscalls/include/syscall.h):

```c
#define SYS_WRITE   1
#define SYS_READ    2   // Chưa implement
#define SYS_YIELD   3
#define SYS_GETPID  4
#define SYS_EXIT    5
```

Tham khảo: Linux AArch64 dùng cùng x8/x0-x5 convention, SYS_WRITE = 64, SYS_READ = 63. ZKOS đơn giản hóa numbering.

---

## Kết quả

```
ZKOS> (task_A đang chạy)
[Task A] Writing via syscall...
[SYS] write(fd=1, count=28) → 28
Hello from Task A via sys_write!
[Task A] My PID = 1
[Task B] Exiting...
[SYS] Task 2 exited.
```

---

## Bài học rút ra

1. **SVC = trap có kiểm soát:** User code không gọi kernel function trực tiếp — dùng `svc #0` để yêu cầu.
2. **EC = 0x15 trong ESR:** Cách kernel phân biệt SVC với crash (Data Abort EC=0x25, Undefined EC=0x00).
3. **x8 cho syscall number:** Convention AArch64 Linux, giữ x0-x5 cho 6 arguments.
4. **Return value qua ctx->x[0]:** Ghi vào saved frame → khi eret, user thấy kết quả trong x0.
5. **ELR tự trỏ đến sau `svc`:** ARM hardware tự save ELR = `PC + 4` khi vào synchronous exception → eret tự động về đúng chỗ.

## Tài liệu đã dùng

| Tài liệu | Phần | Nội dung |
|-----------|------|---------|
| ARM Architecture Reference | D1.10.2 SVC | EC encoding, ISS, ELR behavior |
| ARM Architecture Reference | AAPCS64 Appendix | Syscall calling convention (x8, x0-x5) |
| Linux kernel | `arch/arm64/kernel/sys.c` | AArch64 syscall table pattern |
| VinixOS `kernel/src/kernel/core/svc_handler.c` | | SVC handler tham khảo (ARM32) |
| xv6 book | Chapter 4 (Traps and Syscalls) | Syscall design principles |
