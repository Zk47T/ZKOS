# Bài 2: UART Echo — Input/Output

## Mục tiêu

Nhận ký tự từ serial → gửi lại. Hoàn thiện UART driver với cả TX (truyền) và RX (nhận).

## Tiến triển so với Bài 1

| Bài 1 | Bài 2 |
|-------|-------|
| Chỉ TX (gửi 'Z') | TX + RX (nhận ký tự, gửi lại) |
| Pure ASM hoặc single `*DATA = 'Z'` | Functions: `uart_putc`, `uart_getc`, `uart_puts` |
| In 1 ký tự rồi dừng | Loop vô hạn, interactive |
| 40 bytes | 440+ bytes |

## Kiến thức mới từ RM

### STAT register — thêm bit 21 (RDRF)

**RM Chapter 62 — LPUART, STAT register:**

| Bit | Tên | Ý nghĩa | Dùng khi |
|-----|------|---------|---------|
| 23 | TDRE | Transmit Data Register Empty | Gửi byte — chờ bit này = 1 |
| 21 | RDRF | Receive Data Register Full | Nhận byte — chờ bit này = 1 |

Bài 1 chỉ dùng TDRE (TX). Bài 2 thêm RDRF (RX) — cùng register, khác bit.

### DATA register — đọc vs ghi

Cùng 1 register `DATA` @ offset `0x1C`, nhưng hành vi khác nhau:

| Thao tác | Kết quả |
|----------|---------|
| **Ghi** (`*DATA = 'Z'`) | Gửi byte qua TX pin → serial console |
| **Đọc** (`char c = *DATA`) | Nhận byte từ RX pin ← bàn phím user |

Đây là pattern phổ biến trong MMIO: cùng address, read/write làm khác nhau.

## Code giải thích

### `uart_getc()` — function mới

```c
#define RDRF_BIT (1 << 21)    // RM Ch.62: STAT bit 21

char uart_getc(void)
{
    while ((*STAT & RDRF_BIT) == 0);   // chờ có data từ RX
    return *DATA;                       // đọc byte
}
```

So sánh với `uart_putc()`:

```c
#define TDRE_BIT (1 << 23)    // RM Ch.62: STAT bit 23

void uart_putc(char c)
{
    while ((*STAT & TDRE_BIT) == 0);   // chờ TX sẵn sàng
    *DATA = c;                          // ghi byte
}
```

Cấu trúc giống hệt: chờ status bit → access DATA. Một bên đọc, một bên ghi.

### `uart_puts()` — in chuỗi

```c
void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}
```

Duyệt từng ký tự cho đến `\0` (null terminator). Đơn giản nhưng bắt buộc — bare-metal không có `printf`.

### Line buffer + echo loop

```c
char buf[64];
int i = 0;

while (1) {
    char c = uart_getc();        // nhận 1 ký tự
    if (c == '\r') {             // Enter pressed
        buf[i] = '\0';          // kết thúc chuỗi
        // in kết quả
        uart_puts("[RX] ");
        uart_puts(buf);          // hiển thị dòng đã nhận
        uart_puts("[TX] ");
        uart_puts(buf);          // gửi lại (echo)
        i = 0;                  // reset buffer cho dòng mới
    } else {
        uart_putc(c);            // echo từng ký tự khi gõ
        buf[i++] = c;            // lưu vào buffer
    }
}
```

### Tại sao `\r\n` thay vì chỉ `\n`?

Serial terminal cần cả hai:

| Ký tự | ASCII | Hành vi |
|-------|-------|---------|
| `\r` (CR) | 0x0D | Đưa con trỏ về **đầu dòng** |
| `\n` (LF) | 0x0A | Đưa con trỏ **xuống 1 dòng** |

Chỉ `\n` → con trỏ xuống nhưng không về đầu → text bị lệch.
Di sản từ máy đánh chữ: CR = kéo đầu in sang trái, LF = cuộn giấy lên.

## Output mong đợi

```
Welcome to ZKOS!
ZKOS> hello
[RX] hello
[TX] hello
ZKOS> world
[RX] world
[TX] world
ZKOS> 
```

## Cấu trúc file

```
boot/2-uart-echo/
├── stub.S      ← giống Bài 1 c-lang: set SP, call main
├── main.c      ← uart_putc + uart_getc + uart_puts + echo loop
├── zkos.ld     ← thêm .rodata (cho string constants)
└── Makefile    ← gcc + as + ld + objcopy
```

### Tại sao linker script thêm `.rodata`?

```ld
.text   : { *(.text*) }
.rodata : { *(.rodata*) }     ← MỚI
```

Bài 1 (ASM) không có string → không cần `.rodata`.
Bài 2 có `"Welcome to ZKOS!\r\n"` → gcc đặt string vào section `.rodata`.
Không có dòng này → string mất → `uart_puts` in rác.

## Polling I/O — hạn chế

Code hiện tại dùng **polling**: CPU liên tục kiểm tra `STAT` trong vòng lặp `while`.
Trong khi chờ user gõ, CPU chạy 100% chỉ để hỏi "có data chưa? có data chưa?"

Bài 7 (Interrupts) sẽ chuyển sang **interrupt-driven I/O**: CPU ngủ, UART tự đánh thức CPU khi có data.

## Bài học rút ra

1. **Cùng register, read/write khác nhau** — DATA register: ghi = TX, đọc = RX
2. **Polling = đơn giản nhưng lãng phí CPU** — OK cho bài học, không OK cho production
3. **Line buffer là nền tảng shell** — Bài 4 sẽ thay echo bằng command parsing
4. **`\r\n` bắt buộc với serial** — convention từ thời phần cứng cơ khí

## Tài liệu đã dùng

| Tài liệu | Phần | Nội dung |
|-----------|------|---------|
| i.MX93 RM | Ch.62 LPUART | STAT bit 21 (RDRF), DATA register read behavior |
| Stanford bare-metal guide | gcc flags | Tái khẳng định -ffreestanding, -nostdlib |
