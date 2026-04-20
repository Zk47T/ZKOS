#define LPUART1_BASE  0x44380000
#define STAT_OFFSET   0x14
#define DATA_OFFSET   0x1C
#define TDRE_BIT      (1 << 23)
#define RDRF_BIT      (1 << 21)

volatile int *STAT = (volatile int *)(LPUART1_BASE + STAT_OFFSET);
volatile int *DATA = (volatile int *)(LPUART1_BASE + DATA_OFFSET);

void uart_putc(char c)
{
    while ((*STAT & TDRE_BIT) == 0);
    *DATA = c;
}

char uart_getc(void)
{
    while ((*STAT & RDRF_BIT) == 0);
    return *DATA;
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

void main(void)
{
    char buf[64];
    int i = 0;
    uart_puts("Welcome to ZKOS!\r\n");
    uart_puts("ZKOS> ");
    while (1) {
        char c = uart_getc();
        if (c == '\r') {
            buf[i] = '\0';             // kết thúc chuỗi
            uart_puts("\r\n");
            uart_puts("[RX] ");
            uart_puts(buf);
            uart_puts("\r\n");
            uart_puts("[TX] ");
            uart_puts(buf);
            uart_puts("\r\n");
            uart_puts("ZKOS> ");
            i = 0;                     // reset buffer
        } else {
            uart_putc(c);              // echo từng ký tự khi gõ
            if (i < 63)
                buf[i++] = c;          // lưu vào buffer
        }
    }
}
