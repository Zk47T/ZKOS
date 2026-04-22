# Bài 10 — Preemptive Scheduler: CPU "cướp" quyền điều khiển

Bài 9 xây được multitasking, nhưng chỉ **cooperative** — task phải tự gọi `yield()`. Nếu task quên gọi, hoặc cố tình chiếm CPU, các task khác sẽ bị chết đói. Bài 10 giải quyết: **Timer IRQ tự động kéo task ra khỏi CPU** sau mỗi time slice — không cần sự đồng ý của task đó.

---

## Bối cảnh: Cooperative vs Preemptive

| | Cooperative (Bài 9) | Preemptive (Bài 10) |
|-|---------------------|---------------------|
| Task nhường CPU | Tự gọi `yield()` | Bị timer IRQ buộc switch |
| Task "ích kỷ" có thể chiếm CPU? | Có | Không |
| Độ phức tạp | Thấp | Cao hơn (IRQ context) |
| Ví dụ thực tế | Early DOS, cooperative Windows 3.1 | Linux, macOS, mọi OS hiện đại |

---

## Thiết kế Run Queue và Time Slice

[boot/10-scheduler/kernel/scheduler.c](../boot/10-scheduler/kernel/scheduler.c):

```c
#define TIME_SLICE  10   // 10 ticks × 10ms/tick = 100ms mỗi task

static struct task_struct *run_queue[MAX_TASKS];  // con trỏ đến tasks
static int rq_count = 0;       // số task trong queue
static int current_idx = 0;    // task đang chạy
static int tick_counter = 0;   // đếm ticks từ lần switch cuối
static int started = 0;        // scheduler đã start chưa
```

**Tại sao dùng mảng con trỏ thay vì copy struct?**  
`task_struct` chứa 8KB stack — copy sẽ tốn hơn 8KB mỗi lần thêm task. Mảng con trỏ chỉ tốn 8 bytes/slot.

---

## Từ Timer IRQ đến Context Switch

Đây là luồng chính của preemptive scheduling:

```
Timer hardware hết 10ms
    → GIC kéo IRQ line
    → CPU nhảy vào irq_handler() [main.c]
        → gic_ack_irq()
        → timer_irq_handler() [timer.c]
            → tick_count++
            → re-arm CNTHP_TVAL_EL2
            → scheduler_tick()      ← ĐIỂM KẾT NỐI MỚI
                → tick_counter++
                → nếu tick_counter >= TIME_SLICE:
                    → tick_counter = 0
                    → schedule()    ← chọn task tiếp
        → gic_end_irq()
    ← eret (quay lại task mới)
```

```c
// boot/10-scheduler/kernel/scheduler.c
void scheduler_tick(void)
{
    if (!started) return;  // Không schedule trước khi start

    tick_counter++;
    if (tick_counter >= TIME_SLICE) {
        tick_counter = 0;
        schedule();  // Gọi trực tiếp từ IRQ context
    }
}
```

**Tại sao gọi `schedule()` từ IRQ context có vẻ nguy hiểm nhưng ổn?**  
Mỗi task có stack riêng. Khi IRQ handler chạy, nó đang dùng stack của task hiện tại. `context_switch()` sẽ lưu SP (đang trỏ vào stack IRQ frame của task cũ), rồi switch sang SP của task mới. Khi `eret` thực thi, nó dùng ELR/SPSR đã save từ lúc IRQ vào — CPU quay lại đúng chỗ task mới đã bị dừng. Stack của task cũ còn nguyên, chờ lần sau nó được chạy lại.

---

## Round-Robin Scheduling

```c
// boot/10-scheduler/kernel/scheduler.c
void schedule(void)
{
    if (rq_count <= 1) return;

    int prev_idx = current_idx;
    int next_idx = current_idx;

    // Tìm task READY tiếp theo (bỏ qua DEAD tasks)
    for (int i = 0; i < rq_count; i++) {
        next_idx = (next_idx + 1) % rq_count;
        if (run_queue[next_idx]->state != TASK_DEAD) {
            break;
        }
    }

    if (next_idx == prev_idx) return;

    struct task_struct *prev = run_queue[prev_idx];
    struct task_struct *next = run_queue[next_idx];

    prev->state = TASK_READY;   // Task cũ → sẵn sàng chờ lượt
    next->state = TASK_RUNNING; // Task mới → đang chạy
    current_idx = next_idx;

    context_switch(&prev->ctx, &next->ctx);
    // Từ đây, next task chạy
    // Khi prev task được schedule lại, nó return từ đây
}
```

---

## Khởi động Scheduler

`scheduler_start()` không thể dùng `task_start()` đơn giản nữa — cần trick `dummy context`:

```c
// boot/10-scheduler/kernel/scheduler.c
void scheduler_start(void)
{
    if (rq_count == 0) return;

    started = 1;
    current_idx = 0;
    run_queue[0]->state = TASK_RUNNING;

    /* Jump to first task bằng cách dùng dummy context làm "old" */
    struct task_context dummy;  // Context giả, ta không quan tâm nó bị save gì
    context_switch(&dummy, &run_queue[0]->ctx);
    // Code từ đây không bao giờ return (main() bị "nuốt" vào scheduler loop)
}
```

`main()` gọi `scheduler_start()` và không bao giờ trở về — CPU chạy các tasks từ đó về sau. `main()` stack trở thành "rác" không ai dùng nữa.

---

## Lệnh Shell mới: ps và spawn

```c
// boot/10-scheduler/main.c
else if (strcmp(cmd, "ps") == 0) {
    uart_puts("PID  STATE    NAME\r\n");
    for (int i = 0; i < task_get_count(); i++) {
        struct task_struct *t = task_get(i);
        uart_putint(t->pid);
        uart_puts("    ");
        uart_puts(t->state == TASK_RUNNING ? "RUNNING" :
                  t->state == TASK_READY   ? "READY  " : "DEAD   ");
        uart_puts("  ");
        uart_puts(t->name);
        uart_puts("\r\n");
    }
}
```

---

## Kết quả

```
[SCHED] Scheduler initialized
[SCHED] Added task: shell
[SCHED] Added task: task_A
[SCHED] Added task: task_B
[SCHED] Starting preemptive scheduler

ZKOS> ps
PID  STATE    NAME
0    RUNNING  shell
1    READY    task_A
2    READY    task_B

(Sau 100ms, task_A tự động được schedule mà không cần shell yield)
[A] counter=1000  [A] counter=2000 ...
```

---

## Bài học rút ra

1. **Preemption = Timer IRQ + context_switch:** Không có gì bí ẩn — timer kéo, ta switch, eret trả quyền cho task mới.
2. **Time slice quyết định "cảm giác":** 10ms rất nhanh → mượt. 1000ms → giật. Linux mặc định ~4ms-10ms.
3. **IRQ context vẫn dùng stack của task hiện tại:** Đây là lý do mỗi task cần stack đủ lớn cho cả IRQ frame.
4. **Round-robin đơn giản nhưng không công bằng:** Task tốn nhiều CPU và task nhàn rỗi được cùng time slice. Priority scheduling sẽ giải quyết điều này.
5. **`started` flag quan trọng:** Nếu timer IRQ gọi `schedule()` trước khi tasks được tạo → crash.

## Tài liệu đã dùng

| Tài liệu | Phần | Nội dung |
|-----------|------|---------|
| OSTEP | Chapter 7 (Scheduling) | Round-robin, time slice concepts |
| OSTEP | Chapter 8 (Multi-level Feedback) | Tại sao round-robin không đủ |
| xv6 book | Chapter 7 | Scheduler implementation trong OS thực |
| VinixOS `kernel/src/kernel/scheduler/scheduler.c` | | Round-robin scheduler tham khảo |
| ARM Architecture Reference | D1.7 Interrupt handling | IRQ entry/exit, SPSR/ELR save/restore |
