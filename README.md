# ZKOS — Bare-metal OS cho NXP FRDM-IMX93

ZKOS là dự án tự xây dựng một hệ điều hành tối giản từ đầu, chạy hoàn toàn bare-metal trên **NXP FRDM-IMX93** (Cortex-A55, AArch64). Không RTOS, không thư viện ngoài — từng subsystem được viết tay bằng C và AArch64 assembly với mục đích học sâu về OS internals.

```
ROM → ELE → SPL → TF-A (BL31) → U-Boot → ZKOS (EL2, 0x80400000)
```

---

## 12 Bài học / Subsystems

| # | Tên | Nội dung chính | Trạng thái |
|---|-----|----------------|-----------|
| 1 | **Bare-metal Hello** | Pure ASM (40 bytes) + C stub → in 'Z' qua LPUART1 | ✅ |
| 2 | **UART Echo** | Polling RDRF bit, uart_getc/putc, echo loop | ✅ |
| 3 | **String Library** | strlen, strcmp, memcpy, uart_puthex, tổ chức code | ✅ |
| 4 | **Simple Shell** | Line buffer, command parser, state machine | ✅ |
| 5 | **Timer + Watchdog** | WDOG3 disable, ARM Generic Timer (CNTPCT_EL0), sleep_ms | ✅ |
| 6 | **Exception Vectors** | VBAR_EL2, 16-entry vector table, crash dump 31 regs | ✅ |
| 7 | **Interrupts (GIC)** | GICv3 init (GICD/GICR/ICC), Timer PPI #30 IRQ, DAIF | ✅ |
| 8 | **Memory (MMU)** | 2-level page table, identity mapping, bitmap allocator, kmalloc | ✅ |
| 9 | **Context Switch** | task_struct, callee-save 104B frame, cooperative yield() | ✅ |
| 10 | **Preemptive Scheduler** | Timer IRQ → scheduler_tick → round-robin, ps/spawn | ✅ |
| 11 | **Syscalls** | SVC #0, EC=0x15 dispatch, sys_write/yield/getpid/exit | ✅ |
| 12 | **Filesystem (RAMFS)** | VFS ops table, mount, FD table, ls/cat/write commands | ✅ |

---

## Yêu cầu phần cứng

| Thành phần | Chi tiết |
|-----------|---------|
| **Board** | NXP FRDM-IMX93 (i.MX93, Cortex-A55 dual-core) |
| **Serial console** | USB-to-UART hoặc onboard LPUART1 (115200, 8N1) |
| **SD card** | ≥ 4GB, Class 10 |
| **Máy host** | Linux (Ubuntu/Debian khuyến nghị) |

---

## Cài đặt Toolchain

```bash
# AArch64 bare-metal GCC
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu

# Kiểm tra
aarch64-linux-gnu-gcc --version
```

> **Lưu ý:** Dùng `aarch64-linux-gnu-gcc` với `-ffreestanding -nostdlib` cho bare-metal. Không dùng `aarch64-none-elf-gcc` vì ABI khác nhau.

---

## Build

```bash
# Build một bài cụ thể (ví dụ bài 7)
cd boot/7-interrupts
make clean && make

# Output: zkos.bin (raw binary, ~vài KB)

# Build tất cả
cd boot
make all
```

Mỗi thư mục có `Makefile` và `zkos.ld` riêng. Output là `zkos.bin` — raw binary đặt tại `0x80400000`.

---

## Flash lên SD Card

### Bước 1: Chuẩn bị SD card

> ⚠️ **Thay `/dev/mmcblk0` bằng device đúng của bạn** (kiểm tra bằng `lsblk`).

```bash
# Flash flash.bin (bootloader chain) + zkos.bin lên SD card
cd /path/to/ZKOS
sudo ./scripts/flash.sh /dev/mmcblk0 boot/7-interrupts/zkos.bin
```

Script `flash.sh` tự động:
1. Ghi `blob/flash.bin` tại offset 32KB — BootROM của i.MX93 đọc từ đây
2. Tạo FAT32 partition từ offset 4MB
3. Copy `zkos.bin` và `uboot/boot.scr` lên FAT32

### Bố cục SD Card

```
Byte 0         → MBR (partition table)
Byte 32KB      → flash.bin (ROM → ELE → SPL → TF-A → U-Boot)
Byte 4MB+      → FAT32 partition
  ├── zkos.bin     (ZKOS binary)
  └── boot.scr     (U-Boot auto-boot script)
```

### Bước 2: Kết nối Serial Console

```bash
# Cài minicom hoặc picocom
sudo apt install picocom

# Kết nối (port có thể là /dev/ttyUSB0 hoặc /dev/ttyACM0)
picocom -b 115200 /dev/ttyUSB0
```

Cài đặt: **115200 baud, 8N1, no flow control**.

### Bước 3: Boot

Cắm SD card vào board, bật nguồn. U-Boot sẽ tự load ZKOS nhờ `boot.scr`:

```
U-Boot $ fatload mmc 1:1 0x80400000 zkos.bin
U-Boot $ go 0x80400000
```

Hoặc boot tay trong U-Boot console nếu không có `boot.scr`:

```
=> fatload mmc 1:1 0x80400000 zkos.bin && go 0x80400000
```

### Kết quả mong đợi

```
=============================
  ZKOS v0.7 - Interrupts
=============================
[GIC] GICv3 initialized
[TIMER] EL2 Physical Timer armed
[BOOT] IRQ unmasked
Type 'help' for commands

ZKOS> help
Available commands:
  help   - Show this help message
  info   - System information
  uptime - Show system uptime
  ticks  - Show timer tick count
ZKOS>
```

---

## Cấu trúc thư mục

```
ZKOS/
├── blob/
│   └── flash.bin          ← Prebuilt bootloader chain (ROM→ELE→SPL→TF-A→U-Boot)
├── boot/
│   ├── 1-minimal/         ← Bài 1: in 'Z', ASM và C version
│   ├── 2-uart-echo/       ← Bài 2: polling echo
│   ├── 3-string-lib/      ← Bài 3: string functions
│   ├── 4-simple-shell/    ← Bài 4: shell loop
│   ├── 5-timer/           ← Bài 5: watchdog + generic timer
│   ├── 6-exceptions/      ← Bài 6: vector table + crash dump
│   ├── 7-interrupts/      ← Bài 7: GICv3 + timer IRQ
│   ├── 8-memory/          ← Bài 8: MMU + page alloc + kmalloc
│   ├── 9-context-switch/  ← Bài 9: cooperative multitasking
│   ├── 10-scheduler/      ← Bài 10: preemptive timer-driven scheduler
│   ├── 11-syscalls/       ← Bài 11: SVC syscall layer
│   └── 12-filesystem/     ← Bài 12: VFS + RAMFS
├── docs/
│   ├── 1-minimal.md       ← Tài liệu tiếng Việt, giải thích WHY
│   ├── ...
│   └── 12-filesystem.md
├── scripts/
│   ├── flash.sh           ← Flash SD card (flash.bin + zkos.bin)
│   └── mkbootscr.sh       ← Generate boot.scr từ boot.cmd
└── uboot/
    ├── boot.cmd           ← U-Boot script source
    └── boot.scr           ← Compiled boot script (mkimage)
```

---

## Tài liệu tham khảo

| Tài liệu | Mô tả |
|----------|-------|
| `docs/` folder | Giải thích từng bài bằng tiếng Việt, lý do từng quyết định |
| `FRDM-IMX93-DOCS/` | i.MX93 Reference Manual (RM chapters) và User Manual |
| [ARM Architecture Reference](https://developer.arm.com/documentation/ddi0487/) | AArch64 exception model, page tables, GIC |
| [OSTEP](https://pages.cs.wisc.edu/~remzi/OSTEP/) | OS concepts: scheduling, memory, filesystem |
| [xv6 book](https://pdos.csail.mit.edu/6.828/xv6/book-riscv-rev3.pdf) | Minimal Unix-like OS implementation |

---

## Tác giả

**Nguyen Minh Tien** · [zizuzacker@gmail.com](mailto:zizuzacker@gmail.com) · [embeddedlinux.blog](https://embeddedlinux.blog)
