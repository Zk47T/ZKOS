# Bài 1: Print 'Z' — Bare-metal đầu tiên

## Mục tiêu

In ký tự 'Z' ra serial console từ bare-metal code, không qua OS, không qua thư viện.

## Bối cảnh

Board FRDM-IMX93 đã boot qua chuỗi: ROM → ELE → SPL → TF-A → U-Boot.
U-Boot init xong: DDR, clock, UART, pin mux. Ta chỉ cần ghi vào UART register.

```
ROM → ELE firmware → DDR training → SPL → TF-A (BL31) → U-Boot
                                                              │
                                                    fatload zkos.bin
                                                    go 0x80400000
                                                              │
                                                          ZKOS code ← ta viết phần này
```

## Cái gì đã có sẵn?

U-Boot đã làm hộ ta những thứ sau (ta không cần viết lại):

| Việc | Ai làm | Ở đâu |
|------|--------|-------|
| Init DDR (2GB LPDDR4X) | SPL + Synopsys DDR PHY firmware | flash.bin |
| Init clock tree | SPL | flash.bin |
| Pin mux UART1 TX/RX | U-Boot | flash.bin |
| Set baud rate 115200 | U-Boot | flash.bin |
| Enable LPUART1 TX/RX | U-Boot CTRL register | flash.bin |

## Cần biết gì từ Reference Manual?

### 1. LPUART1 base address

**RM Chapter 2 — Memory Maps:**
```
0x44380000 - 0x4438FFFF    LPUART1    (AONMIX domain)
```

Kiểm chứng: U-Boot boot log ghi `In: serial@44380000`.

### 2. LPUART registers cần dùng

**RM Chapter 62 — LPUART:**

| Offset | Register | Bits quan trọng |
|--------|----------|-----------------|
| `0x14` | STAT | Bit 23: TDRE (Transmit Data Register Empty) — 1 = có thể gửi |
| `0x1C` | DATA | 8 bit thấp: ghi byte để gửi, đọc byte để nhận |

### 3. Load address

**U-Boot source** (`imx93_11x11_frdm_defconfig`):
```
CONFIG_TEXT_BASE=0x80200000         ← U-Boot nằm ở đây
CONFIG_SYS_LOAD_ADDR=0x80400000    ← U-Boot load kernel ở đây
```

`0x80400000` là convention — không phải hardware constraint.

## Giải pháp A: Pure Assembly (40 bytes)

File: `boot/1-minimal/asm-lang/start.S`

```asm
.global _start
_start:
    ldr     x9, =0x44380000       ← LPUART1 base (RM Ch.2)
1:  ldr     w10, [x9, #0x14]      ← đọc STAT register (RM Ch.62)
    tbz     w10, #23, 1b          ← chờ TDRE=1 (TX ready)
    mov     w0, #'Z'              ← ký tự cần gửi
    str     w0, [x9, #0x1C]       ← ghi vào DATA register → UART gửi
2:  wfe                           ← CPU ngủ
    b       2b                    ← loop vô hạn
```

### Tại sao phải dùng Assembly?

Bare-metal không có C runtime. C compiler giả sử:
- Stack pointer (SP) đã hợp lệ → sai, SP trỏ vào stack cũ của U-Boot
- BSS đã được zero → sai, chưa ai zero
- `main()` được gọi bởi `crt0.o` → sai, không có crt0

Assembly là cách duy nhất setup SP trước khi gọi C. Mọi OS (Linux, Zephyr, FreeRTOS) đều bắt đầu từ assembly.

### Build process

```
start.S ──as──→ start.o ──ld──→ zkos.elf ──objcopy──→ zkos-minimal.bin
 (text)    ❶    (ELF)    ❷     (ELF)       ❸         (raw 40 bytes)
```

| Bước | Tool | Làm gì |
|------|------|--------|
| ❶ Assembler | `as` | Dịch mnemonic → machine code |
| ❷ Linker | `ld -T zkos.ld` | Gán address 0x80400000 cho code |
| ❸ Objcopy | `objcopy -O binary` | Bỏ ELF header → raw bytes cho U-Boot |

### Linker script (`zkos.ld`)

```ld
ENTRY(_start)
SECTIONS {
    . = 0x80400000;
    .text : { *(.text*) }
}
```

Tại sao cần: linker mặc định đặt code ở `0x400000` (Linux default). Cần chỉ đúng `0x80400000` (DDR). Nếu sai, absolute address trong code sẽ tính sai.

## Giải pháp B: C + ASM stub (112 bytes)

Cùng chức năng nhưng viết bằng C. Cần 2 file:

### `stub.S` — phần bắt buộc ASM (3 instructions)

```asm
.global _start
_start:
    ldr x0, =0x803F0000    ← stack ở dưới code (0x80400000 - 64KB)
    mov sp, x0              ← set stack pointer
    bl main                 ← gọi C function
```

### `main.c` — logic viết bằng C

```c
#define LPUART1_BASE   0x44380000
#define STAT_OFFSET    0x14
#define DATA_OFFSET    0x1C
#define TDRE_BIT       (1 << 23)

void main(void) {
    volatile int *STAT = (volatile int *)(LPUART1_BASE + STAT_OFFSET);
    volatile int *DATA = (volatile int *)(LPUART1_BASE + DATA_OFFSET);

    while (!(*STAT & TDRE_BIT));
    *DATA = 'Z';
    while (1);
}
```

### GCC flags giải thích

```
gcc -c -ffreestanding -nostdlib -O2 main.c -o main.o
```

| Flag | Tại sao |
|------|---------|
| `-c` | Chỉ compile, link riêng sau (vì cần ghép với stub.o) |
| `-ffreestanding` | Bare-metal: không có printf, malloc, OS |
| `-nostdlib` | Không link libc, không link crt0.o |
| `-O2` | Tối ưu → code nhỏ gọn, gần ASM thủ công |

## Test trên hardware

### Flash SD card

```bash
sudo ./scripts/flash.sh /dev/mmcblk0
```

Script tự động:
1. Ghi `flash.bin` tại offset 32KB (BootROM yêu cầu)
2. Tạo FAT32 partition từ 4MB
3. Copy `zkos-minimal.bin` + `boot.scr` lên FAT32

### SD card layout

```
Byte 0       → MBR (partition table)
Byte 32KB    → flash.bin (ROM đọc trực tiếp từ đây)
Byte 4MB+    → FAT32 partition (zkos-minimal.bin, boot.scr)
```

### Boot

Board tự boot (nhờ `boot.scr`), hoặc gõ tay trong U-Boot:
```
fatload mmc 1:1 0x80400000 zkos-minimal.bin
go 0x80400000
```

Kết quả: ký tự `Z` xuất hiện trên serial console.

## Bài học rút ra

1. **Bare-metal = đọc/ghi register.** Không có API, không có abstraction. Mọi thứ tra từ RM.
2. **Assembly bắt buộc cho entry point** — set SP trước khi C chạy.
3. **Linker script bắt buộc** — nếu sai address, code crash.
4. **U-Boot đã làm 90% công việc khó** — DDR, clock, pin mux. Ta chỉ ghi 1 register.
5. **Alignment quan trọng** — `go 0x80400002` → PC alignment fault ngay lập tức.

## Tài liệu đã dùng

| Tài liệu | Phần | Nội dung |
|-----------|------|---------|
| i.MX93 RM | Ch.2 Memory Maps | LPUART1 base = 0x44380000 |
| i.MX93 RM | Ch.62 LPUART | STAT, DATA register offsets + bit fields |
| U-Boot source | `imx93_frdm_defconfig` | CONFIG_SYS_LOAD_ADDR = 0x80400000 |
| U-Boot boot log | `In: serial@44380000` | Xác nhận UART1 là debug console |
| Stanford bare-metal guide | gcc flags | -ffreestanding, -nostdlib giải thích |
| ARM Architecture Reference | AArch64 ISA | ldr, str, tbz, wfe instruction encoding |
