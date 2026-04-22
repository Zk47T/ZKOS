# Bài 9 — Context Switch: Ảo thuật đa luồng

Bài này là trái tim của mọi OS đa nhiệm. Câu hỏi đặt ra: *Làm sao CPU chỉ có một, nhưng shell và task đếm số có thể chạy "cùng lúc"?* Đây không phải phép màu — đây là **ảo thuật**: CPU lần lượt chạy từng task, chuyển đổi nhanh đến mức người dùng tưởng là song song.

---

## Bối cảnh: Thread là gì?

Bài 4 có một luồng thực thi duy nhất: `main()` → `exec_cmd()` → trở về `main()`. Mọi thứ tuyến tính.

Một **thread** (luồng) là một luồng thực thi độc lập, có:
- **Stack riêng** — biến local, return address của riêng nó
- **Bộ thanh ghi riêng** (đặc biệt `SP`, `PC`, `x19-x30`) — đang tính toán gì, đang ở đâu trong code
- **Trạng thái** (READY, RUNNING, DEAD)

**Context switch** = lưu thanh ghi của task đang chạy vào RAM, nạp thanh ghi của task khác từ RAM → CPU bắt đầu chạy tiếp task mới từ đúng chỗ nó đã dừng.

---

## Task Control Block (TCB)

[boot/9-context-switch/include/task.h](../boot/9-context-switch/include/task.h) định nghĩa `struct task_struct`:

```c
#define TASK_STACK_SIZE  (8 * 1024)  // 8KB stack mỗi task
#define MAX_TASKS        8

typedef enum {
    TASK_READY, TASK_RUNNING, TASK_DEAD
} task_state_t;

struct task_context {
    uint64_t x19, x20, x21, x22, x23, x24;  // callee-saved
    uint64_t x25, x26, x27, x28;             // callee-saved
    uint64_t x29;  // FP (frame pointer)
    uint64_t x30;  // LR (link register = return address)
    uint64_t sp;   // Stack pointer
};  // tổng: 13 × 8 = 104 bytes

struct task_struct {
    int pid;
    const char *name;
    task_state_t state;
    struct task_context ctx;
    uint8_t stack[TASK_STACK_SIZE];  // stack riêng, inline trong TCB
};
```

**Tại sao chỉ save 13 thanh ghi, không phải tất cả 31?**  
Đây là điểm mấu chốt. Theo **AArch64 calling convention** (AAPCS64):

| Nhóm | Thanh ghi | Ai chịu trách nhiệm save? |
|------|-----------|--------------------------|
| Caller-saved | x0-x18 | Hàm GỌI phải save trước khi gọi hàm khác |
| Callee-saved | x19-x30, SP | Hàm ĐƯỢC GỌI phải restore trước khi return |

`context_switch()` là một *hàm được gọi*. Theo convention, CPU đã "biết" rằng x0-x18 có thể bị clobber sau khi gọi hàm. Vì vậy code gọi `context_switch()` không trông đợi x0-x18 còn nguyên. Chỉ cần save x19-x30 và SP là đủ để khi task quay trở lại, mọi thứ đúng như khi nó bị gián đoạn.

---

## Trái tim: context_switch.S

[boot/9-context-switch/kernel/switch.S](../boot/9-context-switch/kernel/switch.S) — 20 dòng ASM làm toàn bộ magic:

```asm
// void context_switch(struct task_context *old, struct task_context *new)
//   x0 = &old->ctx  (lưu vào đây)
//   x1 = &new->ctx  (nạp từ đây)

context_switch:
    // --- SAVE old task ---
    stp x19, x20, [x0, #0x00]   // Store pair: 2 reg → 16 bytes tại offset 0
    stp x21, x22, [x0, #0x10]
    stp x23, x24, [x0, #0x20]
    stp x25, x26, [x0, #0x30]
    stp x27, x28, [x0, #0x40]
    stp x29, x30, [x0, #0x50]   // x30 = LR = địa chỉ return hiện tại của old task
    mov x9, sp
    str x9, [x0, #0x60]          // Save SP

    // --- RESTORE new task ---
    ldp x19, x20, [x1, #0x00]
    ldp x21, x22, [x1, #0x10]
    ldp x23, x24, [x1, #0x20]
    ldp x25, x26, [x1, #0x30]
    ldp x27, x28, [x1, #0x40]
    ldp x29, x30, [x1, #0x50]   // x30 = LR của new task
    ldr x9, [x1, #0x60]
    mov sp, x9                   // Switch stack!

    ret  // jump đến x30 → new task tiếp tục từ chỗ nó đã dừng
```

**`ret` nhảy đến đâu?** `ret` = `br x30`. Sau khi restore, `x30` chứa LR của `new task` — địa chỉ mà task đó đã gọi `context_switch()` từ. Vì vậy new task "thức dậy" ngay tại dòng lệnh sau `context_switch()` trong code của nó — như thể nó chưa từng ngủ.

---

## Khởi tạo task — Trick với LR

[boot/9-context-switch/kernel/task.c](../boot/9-context-switch/kernel/task.c) — hàm `task_create()` setup context ban đầu:

```c
int task_create(const char *name, void (*entry)(void))
{
    struct task_struct *t = &tasks[task_count];

    // Stack của task: mảng uint8_t inline trong struct, SP trỏ đến ĐỠ TRÊN
    uint64_t stack_top = (uint64_t)t->stack + TASK_STACK_SIZE;
    stack_top &= ~0xFUL;  // 16-byte alignment (AArch64 yêu cầu)

    t->ctx.sp  = stack_top;
    t->ctx.x30 = (uint64_t)entry;  // LR = địa chỉ hàm entry!
    t->ctx.x29 = 0;                 // FP = 0 (base frame, không có frame trước)
    // x19-x28 = 0 (giá trị ban đầu không quan trọng)
    ...
}
```

**Trick:** Lần đầu task được schedule, `context_switch()` restore `x30 = entry`. Khi `ret` được thực thi → CPU nhảy đến hàm `entry` của task. Task bắt đầu chạy từ đầu hàm của nó, với SP là đỉnh stack riêng — chính xác như một hàm mới được gọi.

---

## Cooperative Scheduling với yield()

Bài 9 dùng **cooperative scheduling** — task tự nguyện nhường CPU:

```c
// boot/9-context-switch/kernel/task.c
void yield(void)
{
    int prev = current_task;
    int next = (prev + 1) % task_count;  // Round-robin đơn giản

    // Tìm task tiếp theo READY
    while (tasks[next].state != TASK_READY && next != prev)
        next = (next + 1) % task_count;

    if (next == prev) return;

    tasks[prev].state = TASK_READY;
    tasks[next].state = TASK_RUNNING;
    current_task = next;

    context_switch(&tasks[prev].ctx, &tasks[next].ctx);
    // Sau khi return từ context_switch ở đây, prev task tiếp tục
}
```

**Hành vi từ góc nhìn task A:**
```
task_A() gọi yield()
  → yield() gọi context_switch(&taskA.ctx, &taskB.ctx)
  → CPU bây giờ đang chạy task_B
  → (task_B chạy một lúc, rồi nó gọi yield())
  → context_switch(&taskB.ctx, &taskA.ctx) được gọi
  → context_switch() trong task_A *return*
  → yield() trong task_A return
  → task_A tiếp tục từ dòng sau yield()
```

---

## Khởi động task đầu tiên

`context_switch()` cần một "old" context để save vào. Task đầu tiên không có "previous" task. `task_start()` giải quyết bằng cách manually load context:

```c
void task_start(void)
{
    current_task = 0;
    tasks[0].state = TASK_RUNNING;

    struct task_context *ctx = &tasks[0].ctx;
    asm volatile(
        "mov sp, %0\n"   // Switch sang stack của task 0
        "br %1\n"        // Nhảy thẳng đến entry của task 0
        :: "r"(ctx->sp), "r"(ctx->x30)
    );
    // Code từ đây không bao giờ chạy
}
```

Không dùng `context_switch()` vì không cần save "old" task — đây là điểm bắt đầu tuyệt đối.

---

## Kết quả

```
[TASK] Created task 0: 'shell'
[TASK] Created task 1: 'task_A'
[TASK] Created task 2: 'task_B'
ZKOS> 
[A] tick=1  [B] tick=1  [A] tick=2  [B] tick=2  ...
```

Shell, task_A, task_B luân phiên nhau — mỗi khi gọi `yield()`.

---

## Bài học rút ra

1. **Thread = stack riêng + context riêng.** Đây là toàn bộ bí mật của multitasking.
2. **Chỉ save callee-saved registers:** Convention của ABI giúp giảm 31 → 13 registers cần save.
3. **LR là "bookmark":** x30 lưu địa chỉ return — context switch lợi dụng điều này để task "thức dậy" đúng chỗ.
4. **Cooperative vs Preemptive:** Bài 9 dùng cooperative (task tự `yield()`). Bài 10 sẽ dùng timer IRQ để preempt cưỡng bức.
5. **Stack overflow là nguy hiểm thật:** 8KB mỗi task, không có guard page (chưa có MMU page fault xử lý). Đây là lý do OS thật dùng guard page.

## Tài liệu đã dùng

| Tài liệu | Phần | Nội dung |
|-----------|------|---------|
| ARM Architecture Reference | AAPCS64 | Calling convention, callee vs caller saved |
| ARM Architecture Reference | A1.4 | stp/ldp instruction encoding |
| OSTEP | Chapter 26 (Concurrency Intro) | Thread, context switch concept |
| xv6 book | Chapter 7 (Scheduling) | Context switch implementation tham khảo |
| VinixOS `arch/arm/scheduler/context_switch.S` | | ARM32 context switch pattern |
