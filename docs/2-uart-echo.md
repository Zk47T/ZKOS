Ở bài trước, mình đã in được ký tự Z ra serial console. Bài này mình sẽ làm ngược lại, nhận ký tự từ bàn phím và gửi trả lại (echo). Đây là bước đầu để làm shell sau này.

1. Bài này cần gì thêm từ RM?1.1 Thêm bit RDRF để nhận data1.2 DATA register đọc vs ghi2. Code C2.1 uart_getc() — nhận ký tự2.2 uart_puts() — in chuỗi2.3 Echo loop với line buffer3. Vụ \r\n4. Output mong đợi5. Polling I/O

1. Bài này cần gì thêm từ RM?

1.1 Thêm bit RDRF để nhận data

Bài 1 mình chỉ dùng 1 bit trong STAT register: TDRE (bit 23) để biết khi nào gửi được. Bài này cần thêm 1 bit nữa: RDRF (bit 21)

Vẫn là RM Chapter 62, STAT register:
- Bit 23: TDRE (Transmit Data Register Empty) — 1 = có thể gửi byte
- Bit 21: RDRF (Receive Data Register Full) — 1 = có byte mới để đọc

1.2 DATA register đọc vs ghi

Cái hay ở đây là cùng 1 register DATA (offset 0x1C), nhưng đọc và ghi sẽ khác nhau:
- Ghi vào DATA (*data = 'Z') → gửi byte qua TX pin → serial console
- Đọc từ DATA (char c = *data) → nhận byte từ RX pin ← bàn phím user

Đây là pattern phổ biến trong MMIO (Memory-Mapped I/O): cùng address, read/write làm việc khác nhau.

2. Code C

Các bạn có thể tham khảo tại 2-uart-echo

Mình giữ nguyên stub.S và Makefile từ bài 1 c-lang, chỉ thay main.c

2.1 uart_getc() — nhận ký tự

#define RDRF_BIT (1 << 21)

char uart_getc(void)
{
    while ((*stat & RDRF_BIT) == 0);
    return *data;
}

Nếu bạn so sánh với uart_putc() ở bài 1:

void uart_putc(char c)
{
    while ((*stat & TDRE_BIT) == 0);
    *data = c;
}

Cấu trúc y hệt: chờ status bit → access DATA. Một bên đọc, một bên ghi. Nếu hiểu uart_putc thì uart_getc cũng tương tự thôi.

2.2 uart_puts() — in chuỗi

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

Bare-metal không có printf, nên muốn in chuỗi thì phải tự viết. Đơn giản duyệt từng ký tự cho đến null terminator.

Lưu ý vì có string constant ("Welcome to ZKOS!\r\n") nên gcc sẽ đặt nó vào section .rodata. Do đó linker script phải thêm dòng:

.rodata : { *(.rodata*) }

Bài 1 asm không có string nên không cần. Bài 1 c-lang thì chỉ có 'Z' (ký tự, không phải string) nên cũng không cần. Bài này bắt đầu cần.

2.3 Echo loop với line buffer

void main(void)
{
    char buf[64];
    int i = 0;
    uart_puts("Welcome to ZKOS!\r\n");
    uart_puts("ZKOS> ");
    while (1) {
        char c = uart_getc();
        if (c == '\r') {
            buf[i] = '\0';
            uart_puts("\r\n");
            uart_puts("[RX] ");
            uart_puts(buf);
            uart_puts("\r\n");
            uart_puts("[TX] ");
            uart_puts(buf);
            uart_puts("\r\n");
            uart_puts("ZKOS> ");
            i = 0;
        } else {
            uart_putc(c);
            if (i < 63)
                buf[i++] = c;
        }
    }
}

Ở đây mình không echo từng ký tự rồi thôi, mà buffer cả dòng lại. Khi user ấn Enter (\r), mình in ra [RX] (đã nhận) và [TX] (gửi lại). Cái line buffer này chính là nền tảng cho shell sau này, thay vì echo lại thì mình sẽ parse command.

3. Vụ \r\n

Khi bạn ấn Enter trên serial terminal (minicom, picocom), terminal gửi \r (0x0D = Carriage Return).

Nhưng để con trỏ xuống dòng đúng trên terminal, cần gửi cả hai:
- \r (CR): đưa con trỏ về đầu dòng
- \n (LF): đưa con trỏ xuống 1 dòng

Nếu chỉ gửi \n thì con trỏ xuống dòng nhưng không về đầu, text sẽ bị lệch sang phải. Đây là di sản từ thời máy đánh chữ: carriage return = kéo đầu in về trái, line feed = cuộn giấy lên.

4. Output mong đợi

Make rồi flash vào board:

sudo ./scripts/flash.sh blob/flash.bin boot/2-uart-echo/zkos.bin /dev/sdX

Ta sẽ thấy:

## Starting application at 0x80400000 ...
Welcome to ZKOS!
ZKOS> hello
[RX] hello
[TX] hello
ZKOS> world
[RX] world
[TX] world
ZKOS> 

5. Polling I/O

Code hiện tại dùng polling: CPU liên tục check STAT register trong vòng lặp while. Trong khi chờ user gõ, CPU chạy 100% chỉ để hỏi "có data chưa? có data chưa?"

Tạm thời thì ok cho bài học, nhưng production thì không. Bài sau mình sẽ dần chuyển sang interrupt-driven I/O: CPU ngủ, UART tự đánh thức CPU khi có data.

Ngoài ra thì board cũng sẽ tự reset sau ~40s do watchdog (WDOG3). Vụ này mình sẽ xử lý ở bài sau luôn.

Ở bài tới, mình sẽ tách code ra thành nhiều file (uart.c, string.c) cho gọn, rồi bắt đầu làm shell đơn giản — gõ "help" ra danh sách lệnh, "hello" ra chào.
