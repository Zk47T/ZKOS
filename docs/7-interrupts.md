# Bài 7 — Interrupts: Thoát khỏi vòng lặp bận rộn

Từ bài 1 đến bài 6, mọi thứ ZKOS làm đều theo mô hình **polling**: CPU liên tục kiểm tra một bit, chờ đến khi bit đó thay đổi mới làm gì tiếp. Bài này đặt ra câu hỏi: *CPU đang chạy shell thì đồng hồ 10ms của hệ thống làm sao kêu được?* Câu trả lời là **interrupt** — phần cứng chủ động gõ cửa CPU, không cần CPU hỏi.

---

## Bối cảnh: Polling là gì và tại sao cần interrupt?

Bài 5 đo thời gian bằng cách đọc `CNTPCT_EL0` liên tục trong vòng `while`. CPU bận 100% chỉ để nhìn kim đồng hồ. Nếu ta muốn timer tăng `tick_count` *đồng thời* với shell đang chờ gõ phím, polling không làm được — CPU chỉ có thể làm một việc tại một thời điểm.

Interrupt giải quyết bằng cách: **Timer phần cứng tự đếm, khi hết thời gian thì tự kéo chân tín hiệu (IRQ line)**. CPU đang làm gì cũng phải dừng lại, nhảy vào hàm xử lý (IRQ handler), xong rồi quay lại tiếp tục công việc cũ.

```
CPU đang chạy shell...
                    ┌─── Timer hết 10ms → kéo IRQ line ───┐
                    │                                      │
                    ▼                                      │
           Dừng shell, nhảy vào irq_handler()             │
           → tăng tick_count                              │
           → re-arm timer                                 │
           ← eret (quay lại shell đúng chỗ đã dừng) ────┘
```

---

## Kiến trúc interrupt trên i.MX93

i.MX93 dùng **GICv3** (Generic Interrupt Controller version 3) — chuẩn ARM, không phải chip-specific như INTC của AM335x trong VinixOS.

GICv3 có 3 khối chức năng:

| Khối | Địa chỉ (RM Ch.3) | Vai trò |
|------|-------------------|---------|
| **GICD** (Distributor) | `0x48000000` | Nhận tín hiệu IRQ từ thiết bị, phân phối đến CPU core |
| **GICR** (Redistributor) | `0x48040000` | Quản lý IRQ riêng cho từng CPU core, bật/tắt SGI/PPI |
| **ICC** (CPU Interface) | System registers | CPU đọc/xác nhận IRQ qua system registers (không phải MMIO) |

### Phân loại interrupt (INTID)

| Loại | INTID | Ví dụ |
|------|-------|-------|
| SGI (Software Generated) | 0-15 | IPI giữa các core |
| PPI (Per-Peripheral Interface) | 16-31 | Timer riêng của từng core |
| SPI (Shared Peripheral) | 32-1019 | UART, GPIO... |

Timer IRQ của Bài 7 dùng PPI #30 — **EL2 Physical Timer** (`CNTHP_TVAL_EL2`), PPI nên nó "riêng" cho core 0, không cần cấu hình GICD_ISENABLER mà dùng GICR_ISENABLER0.

---

## Quá trình suy luận và thực hiện

### Bước 1: Khởi động GICv3 — 5 việc phải làm đúng thứ tự

Nhìn vào [boot/7-interrupts/drivers/gic.c](../boot/7-interrupts/drivers/gic.c), hàm `gic_init()` thực hiện đúng 5 bước mà GICv3 spec yêu cầu:

```c
// Bước 1: Bật System Register interface tại EL2
//   ICC_SRE_EL2: bit0=SRE (dùng system reg, không dùng MMIO), bit1=DFB, bit2=DIB
WRITE_SYSREG(S3_4_C12_C9_5, 0x7);
asm volatile("isb");  // Bắt buộc sau khi thay đổi ICC registers

// Bước 2: GICD — bật Affinity Routing + Group 1 NS
GICD_CTLR = GICD_CTLR_ARE_NS | GICD_CTLR_ENGRP1NS;

// Bước 3: GICR — đánh thức PE (Processor Element)
//   Xóa bit ProcessorSleep, chờ ChildrenAsleep về 0
GICR_WAKER &= ~GICR_WAKER_PS;
while (GICR_WAKER & GICR_WAKER_CA) {}  // spin

// Bước 4: CPU Interface — chấp nhận mọi mức priority (0xFF = không chặn gì)
WRITE_SYSREG(S3_0_C4_C6_0, 0xFF);  // ICC_PMR_EL1

// Bước 5: CPU Interface — bật Group 1 interrupts
WRITE_SYSREG(S3_0_C12_C12_7, 1);   // ICC_IGRPEN1_EL1
```

**Tại sao dùng system registers thay vì MMIO cho CPU Interface?**  
GICv3 bỏ hẳn memory-mapped CPU Interface từ spec. `S3_4_C12_C9_5` là encoding thủ công của `ICC_SRE_EL2` vì GCC chưa biết tên này. Đây là điểm khác lớn so với GICv2 (VinixOS dùng GICv2 với MMIO hoàn toàn).

### Bước 2: Bật timer IRQ

[boot/7-interrupts/drivers/timer.c](../boot/7-interrupts/drivers/timer.c) thêm hàm `timer_init_irq()`:

```c
void timer_init_irq(uint32_t interval_ms)
{
    uint64_t freq = get_timer_frequency();  // đọc CNTFRQ_EL0
    timer_interval_ticks = (freq * interval_ms) / 1000;

    // Nạp countdown value vào EL2 Physical Timer
    WRITE_SYSREG(cnthp_tval_el2, timer_interval_ticks);

    // CNTHP_CTL_EL2: bit0=ENABLE, bit1=IMASK (0=không mask)
    WRITE_SYSREG(cnthp_ctl_el2, 1);  // bật, không mask
}
```

**Tại sao dùng CNTHP (EL2 Physical Timer)?**  
ZKOS chạy ở EL2. ARM Generic Timer có 4 loại timer cho các EL khác nhau:
- `CNTPS` — EL3 Physical Secure
- `CNTHP` — EL2 Physical (ta dùng)
- `CNTP` — EL1 Physical  
- `CNTV` — EL1 Virtual

Dùng `CNTHP` là chọn đúng timer cho EL2, tránh xung đột với EL1/EL0.

### Bước 3: Mở khóa DAIF — cánh cửa cuối cùng

CPU ARM có một cơ chế bảo vệ: **DAIF** (Debug, Abort, IRQ, FIQ) — 4 bit mask trong thanh ghi PSTATE. Bit I=1 nghĩa là chặn IRQ. Ngay cả khi GIC đã cấu hình đúng, nếu DAIF.I = 1 thì IRQ vẫn không vào được CPU.

```c
// Trong main.c, sau khi GIC và timer đã init xong:
asm volatile("msr daifclr, #2");  // #2 = clear bit I → mở cửa cho IRQ
```

**Thứ tự quan trọng:** Init GIC → Init timer → Mở DAIF. Nếu mở DAIF trước khi GIC init xong, IRQ có thể đến khi handler chưa sẵn sàng → crash.

### Bước 4: Vectors.S — thêm IRQ entry

Bài 6 chỉ xử lý synchronous exception. Bài 7 cần thêm IRQ handler vào vector table. IRQ từ EL2 với SPx rơi vào entry thứ 6 (offset `0x280`):

```asm
// boot/7-interrupts/vectors.S
.align 7
handle_irq_el2:
    sub sp, sp, #272
    stp x0, x1, [sp, #0]
    // ... save tất cả regs ...
    mov x0, sp
    bl irq_handler      // Gọi C handler
    // restore regs
    ldp x0, x1, [sp, #0]
    // ...
    add sp, sp, #272
    eret
```

### Bước 5: IRQ Handler — ack → xử lý → EOI

[boot/7-interrupts/main.c](../boot/7-interrupts/main.c) có `irq_handler()`:

```c
void irq_handler(void)
{
    uint32_t irq_id = gic_ack_irq();   // Đọc ICC_IAR1_EL1 → lấy INTID

    if (irq_id == TIMER_IRQ_ID) {      // PPI #30
        timer_irq_handler();            // tăng tick_count, re-arm timer
    }

    gic_end_irq(irq_id);  // Ghi ICC_EOIR1_EL1 → báo GIC "đã xử lý xong"
}
```

**Tại sao phải gọi `gic_end_irq()` (EOI)?**  
GIC có hai chế độ hoàn thành interrupt. Ở mode mặc định (split), `ICC_IAR1_EL1` = "bắt đầu xử lý" và `ICC_EOIR1_EL1` = "đã xong". Nếu không gọi EOI, GIC không gửi IRQ tiếp theo cùng priority → timer chỉ kêu 1 lần rồi im.

---

## Kết quả

```
ZKOS v0.7 - Interrupts
[GIC] Initializing GICv3...
[GIC] GICv3 initialized
[TIMER] Freq: 24000000 Hz, interval: 10 ms
[TIMER] EL2 Physical Timer armed
[BOOT] IRQ unmasked
ZKOS> ticks
Timer ticks: 47 (10ms each)
ZKOS> uptime
Uptime: 2 seconds
```

Shell vẫn chạy bình thường, trong khi timer IRQ tự động tăng `tick_count` mỗi 10ms — không cần CPU polling.

---

## Bài học rút ra

1. **GICv3 = 3 khối phải init đúng thứ tự:** GICD → GICR → ICC system registers → DAIF.
2. **PPI vs SPI:** Timer của mỗi core là PPI, cấu hình qua GICR (redistributor), không phải GICD.
3. **DAIF là cánh cửa cuối:** GIC sẵn sàng nhưng CPU vẫn chặn IRQ cho đến khi `msr daifclr, #2`.
4. **Ack + EOI bắt buộc:** Thiếu một trong hai → GIC block → interrupt chỉ đến một lần.
5. **EL2 Physical Timer (CNTHP):** Mỗi EL có timer riêng, chọn đúng EL hiện tại.

## Tài liệu đã dùng

| Tài liệu | Phần | Nội dung |
|-----------|------|---------|
| i.MX93 RM | Ch.3 Interrupt/DMA | GIC-400 base addresses, INTID mapping |
| ARM GICv3 Architecture Spec | | GICD/GICR/ICC register layout, init sequence |
| ARM Architecture Reference | Ch. Generic Timer | CNTHP_TVAL_EL2, CNTHP_CTL_EL2, DAIF |
| VinixOS `kernel/src/drivers/intc.c` | | GIC init pattern tham khảo (GICv2) |
