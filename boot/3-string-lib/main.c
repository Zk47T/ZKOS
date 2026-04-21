#include "uart.h"
#include "string.h"

void main(void)
{
    char buf[64];
    int i =0;

    uart_puts("Welcome to ZKOS \r\n");
    uart_puts("Test puts hex\r\n");
    uart_puthex(0x43880000);
    uart_puts("\r\n");

    uart_puts("Test memcpy\r\n");

    char test_memcpy[16];
    memcpy(test_memcpy, "MemcpyOK",9);
    uart_puts(test_memcpy);
    uart_puts("\r\n");

    uart_puts("Test memset\r\n");
    char test_memset[16];
    memset(test_memset, 'Z',5);
    test_memset[5]='\0';
    uart_puts(test_memset);

    uart_puts("\r\n\r\n");
    uart_puts("ZKOS> ");

    while(1)
    {
        char c = uart_getc();
        if(c == '\r')
        {
            buf[i] = '\0';
            buf[i] = '\0';
            uart_puts("\r\n");
            uart_puts("[RX] ");
            uart_puts(buf);
            uart_puts(" (len=");
            uart_puthex(strlen(buf));
            uart_puts(")\r\n");
            uart_puts("[TX] ");
            uart_puts(buf);
            uart_puts("\r\n");
            uart_puts("ZKOS> ");
            i = 0;
        }
        else
        {
            uart_putc(c);
            if(i < 63)
            {
                buf[i++] = c;
            }
        }
    }
}