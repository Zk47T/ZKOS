# ASM Minimal Guide

Giải thích 3 files trong `boot/minimal/asm-lang/` — in ký tự 'Z' ra UART.

---

## start.S

```asm
.global _start
_start:
    ldr     x9, =0x44380000        ❶
1:  ldr     w10, [x9, #0x14]       ❷
    tbz     w10, #23, 1b           ❸
    mov     w0, #'Z'               ❹
    str     w0, [x9, #0x1C]        ❺
2:  wfe                            ❻
    b       2b                     ❼
```

| # | Instruction | Làm gì | Lấy từ đâu trong RM |
|---|-------------|--------|---------------------|
| ❶ | `ldr x9, =0x44380000` | Nạp địa chỉ LPUART1 vào x9 | RM Ch.2 — Memory Map |
| ❷ | `ldr w10, [x9, #0x14]` | Đọc register STAT (base + 0x14) | RM Ch.62 — LPUART register map |
| ❸ | `tbz w10, #23, 1b` | Nếu bit 23 (TDRE) = 0 → quay lại ❷ | RM Ch.62 — STAT bit 23 = TX ready |
| ❹ | `mov w0, #'Z'` | Đặt ASCII 'Z' (0x5A) vào w0 | — |
| ❺ | `str w0, [x9, #0x1C]` | Ghi vào register DATA (base + 0x1C) → UART gửi | RM Ch.62 — DATA register |
| ❻ | `wfe` | CPU ngủ (Wait For Event) | — |
| ❼ | `b 2b` | Nhảy lại ❻ → ngủ mãi | — |

### `.global _start` là gì?

Nói cho linker biết symbol `_start` có thể nhìn thấy từ bên ngoài file.
Nếu không có dòng này, linker không tìm được `_start` → báo lỗi `undefined reference`.

### `1:` và `2:` là gì?

Label số (local label). `1b` = nhảy về label `1` phía **b**efore (trước).
Dùng số thay vì đặt tên vì có thể tái sử dụng — hai chỗ khác nhau đều có thể có `1:`.

### `w` vs `x` register?

ARM64 có 31 register, mỗi register 64-bit:
- `x0` - `x30` = truy cập đầy đủ 64-bit
- `w0` - `w30` = chỉ 32-bit thấp (half)

LPUART register là 32-bit → dùng `w`. Địa chỉ là 64-bit → dùng `x`.

---

## zkos.ld

```ld
ENTRY(_start)
SECTIONS {
    . = 0x80400000;
    .text : { *(.text*) }
}
```

### Tại sao vẫn cần file này?

**Không có linker script → linker dùng default → code đặt ở 0x0 → sai.**

Khi U-Boot chạy `go 0x80400000`, nó nhảy đến DDR address 0x80400000.
Linker cần biết "code sẽ nằm ở 0x80400000" để tính đúng mọi absolute address trong binary.

### Thử nghiệm: bỏ linker script

```bash
aarch64-linux-gnu-ld -nostdlib start.o -o test.elf
aarch64-linux-gnu-objdump -d test.elf | head
```

Bạn sẽ thấy `_start` nằm ở `0x0000000000400000` (default) thay vì `0x80400000`.
Với code minimal hiện tại (chỉ có PC-relative instructions), nó vẫn chạy được vì `ldr`, `tbz`, `b` đều tính offset tương đối.

**Nhưng khi code lớn hơn** (có biến global, string, function pointer), absolute address sẽ sai → crash.
Nên luôn dùng linker script — thêm 4 dòng nhưng đảm bảo đúng mọi trường hợp.

### Từng dòng

| Dòng | Ý nghĩa |
|------|---------|
| `ENTRY(_start)` | Symbol đầu tiên trong binary. `objcopy` đặt byte của `_start` ở đầu file |
| `. = 0x80400000` | Bắt đầu section đầu tiên ở địa chỉ này. `.` = "con trỏ vị trí hiện tại" |
| `.text : { *(.text*) }` | Gom tất cả machine code từ mọi `.o` file vào section `.text` |

---

## Makefile

```makefile
all:
	aarch64-linux-gnu-as start.S -o start.o
	aarch64-linux-gnu-ld -T zkos.ld -nostdlib start.o -o zkos.elf
	aarch64-linux-gnu-objcopy -O binary zkos.elf zkos-minimal.bin

clean:
	rm -f start.o zkos.elf zkos-minimal.bin
```

### 3 bước build

```
start.S ──as──→ start.o ──ld──→ zkos.elf ──objcopy──→ zkos-minimal.bin
 (text)         (ELF)          (ELF)                   (raw bytes)
```

| Bước | Tool | Input → Output | Làm gì |
|------|------|---------------|--------|
| 1 | `as` (assembler) | `.S` → `.o` | Dịch mnemonic (`ldr`, `str`) thành mã máy |
| 2 | `ld` (linker) | `.o` → `.elf` | Sắp xếp code theo zkos.ld, resolve symbol |
| 3 | `objcopy` | `.elf` → `.bin` | Bỏ ELF header, chỉ giữ raw machine code |

### Tại sao cần bước 3?

U-Boot `go 0x80400000` nhảy thẳng đến byte đầu tiên và thực thi.
Nó không hiểu ELF format. Nếu load file `.elf`:

```
Byte đầu = 0x7F 'E' 'L' 'F'  ← ELF magic header
                                 CPU hiểu đây là instruction gì? → crash
```

Flat binary:
```
Byte đầu = instruction đầu tiên của _start → CPU chạy đúng
```

### `-nostdlib` là gì?

Nói cho linker: không link C standard library (libc).
Bare-metal không có `printf`, `malloc`, `exit` — chỉ có code bạn tự viết.

### `-T zkos.ld` là gì?

`-T` = dùng linker script thay thế luật mặc định.
Không có `-T` → linker dùng script mặc định → code ở sai địa chỉ.
